#include "client/network_processing.hpp"

#ifdef _WIN32

#include <iostream>
#include <mutex>

#include "client/process_stat.hpp"
#include "common/util.hpp"
#include "common/network.hpp"

DecoderContext::DecoderContext(_In_opt_ LPCWSTR szTmfSearchPath) {
	TDH_CONTEXT* p = m_tdhContext;

	if (szTmfSearchPath != nullptr) {
		p->ParameterValue = reinterpret_cast<UINT_PTR>(szTmfSearchPath);
		p->ParameterType = TDH_CONTEXT_WPP_TMFSEARCHPATH;
		p->ParameterSize = 0;
		p += 1;
	}

	m_tdhContextCount = static_cast<BYTE>(p - m_tdhContext);
}

namespace {
static std::mutex pid_network_stat_map_mutex;
static std::unordered_map<std::uint32_t, NetworkStat> pid_network_stat_map;

static void WINAPI OnEvent(PEVENT_RECORD rec) {
	try {
		DecoderContext pContext = DecoderContext((LPCWSTR)rec->UserContext);
		NetworkPacketInfo network_packet_info = pContext.get_network_packet(rec);
		std::lock_guard<std::mutex> lock(pid_network_stat_map_mutex);
		NetworkStat& network_stat = pid_network_stat_map[network_packet_info.pid];
		if (network_packet_info.receive == true) {
			network_stat.network_recv = network_stat.network_recv.value_or(0) + network_packet_info.size;
		} else {
			network_stat.network_send = network_stat.network_send.value_or(0) + network_packet_info.size;
		}
	} catch (std::exception const& ex) {
		std::wcerr << L"OnEvent: error: " << ex.what() << L"\n";
	}
}

// blocks
static void start_trace() {
	GUID guid;
	CLSIDFromString(L"{7dd42a49-5329-4832-8dfd-43d979153a88}", &guid); // Microsoft-Windows-Kernel-Network
	WCHAR session_name[] = L"RMTNetworkSession";
	TRACEHANDLE trace_handle;
	auto size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(session_name);
	auto buffer = std::make_unique<uint8_t[]>(size);
	auto properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.get());
	memset(properties, 0, size);
	properties->Wnode.BufferSize = size;
	properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

	auto status = StartTraceW(&trace_handle, session_name, properties);
	if (status == ERROR_ALREADY_EXISTS) {
		ControlTraceW(0, session_name, properties, EVENT_TRACE_CONTROL_STOP);

		// Re-initialize properties before calling StartTrace again
		memset(properties, 0, size);
		properties->Wnode.BufferSize = static_cast<ULONG>(size);
		properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
		properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		status = StartTraceW(&trace_handle, session_name, properties);
	}

	if (status != ERROR_SUCCESS) {
		std::cerr << "start_trace_w: error" << status << "\n";
		return;
	}
	status =
	EnableTraceEx2(trace_handle, &guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE,
		0,      // MatchAnyKeyword - 0 means all keywords
		0,      // MatchAllKeyword
		0,      // Timeout
		nullptr // EnableParameters
	);
	if (status != ERROR_SUCCESS) {
		std::cerr << "enable_trace_ex2: error" << status << "\n";
		return;
	}
	
	EVENT_TRACE_LOGFILEW etl{};
	etl.LoggerName = const_cast<PWSTR>(session_name);
	etl.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
	etl.EventRecordCallback = OnEvent;
	TRACEHANDLE hParse = OpenTraceW(&etl);
	if (hParse == INVALID_PROCESSTRACE_HANDLE) {
		std::cerr << "open trace: error << " << GetLastError() << "\n";
		return;
	}
	std::cerr << "start trace ok, now process trace\n";
	// blocks
	ProcessTrace(&hParse, 1, nullptr, nullptr);
}
} // namespace

NetworkPacketInfo DecoderContext::get_network_packet(_In_ PEVENT_RECORD pEventRecord) {
	BYTE const* pb_data = static_cast<BYTE const*>(
	    pEventRecord->UserData); // Position of the next byte of event data to be consumed.
	BYTE const* pb_data_end =
	    pb_data + pEventRecord->UserDataLength; // // Position of the end of the event data.
	BYTE m_pointerSize = pEventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER ? 4
	                     : pEventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_64_BIT_HEADER
	                         ? 8
	                         : sizeof(void*); // Ambiguous, assume size of the decoder's pointer.

	ULONG status;
	ULONG cb;

	// Try to get event decoding information from TDH.
	cb = static_cast<ULONG>(m_teiBuffer.size());
	status = TdhGetEventInformation(pEventRecord, m_tdhContextCount,
	                                m_tdhContextCount ? m_tdhContext : nullptr,
	                                reinterpret_cast<TRACE_EVENT_INFO*>(m_teiBuffer.data()), &cb);
	if (status == ERROR_INSUFFICIENT_BUFFER) {
		m_teiBuffer.resize(cb);
		status = TdhGetEventInformation(
		    pEventRecord, m_tdhContextCount, m_tdhContextCount ? m_tdhContext : nullptr,
		    reinterpret_cast<TRACE_EVENT_INFO*>(m_teiBuffer.data()), &cb);
	}

	TRACE_EVENT_INFO const* const pTei =
	    reinterpret_cast<TRACE_EVENT_INFO const*>(m_teiBuffer.data());

	std::wstring keyword_name = this->TeiString(pTei->KeywordsNameOffset);

	NetworkPacketInfo network_packet;
	if (keyword_name != std::wstring(L"KERNEL_NETWORK_KEYWORD_IPV4")) {
		std::wcerr << L"decoder context: got " << keyword_name << L"\n";
		return network_packet;
	}

	std::vector<wchar_t> m_propertyBuffer; // Buffer for the string returned by TdhFormatProperty.


	std::wstring opcode_name = this->TeiString(pTei->OpcodeNameOffset);

	// there are more types other than just data sent and received
	if (opcode_name.find(L"Data sent") != std::wstring::npos) {
		network_packet.receive = false;
	} else if (opcode_name.find(L"Data received") != std::wstring::npos) {
		network_packet.receive = true;
	}

	for (unsigned prop_index = 0; prop_index != pTei->TopLevelPropertyCount; prop_index += 1) {
		EVENT_PROPERTY_INFO const& epi = pTei->EventPropertyInfoArray[prop_index];

		for (;;) {
			ULONG cbBuffer = static_cast<ULONG>(m_propertyBuffer.size() * 2);
			USHORT cbUsed = 0;
			ULONG status;

			status = TdhFormatProperty(
			    const_cast<TRACE_EVENT_INFO*>(pTei), nullptr, m_pointerSize,
			    epi.nonStructType.InType,
			    static_cast<USHORT>(epi.nonStructType.OutType == TDH_OUTTYPE_NOPRINT
			                            ? TDH_OUTTYPE_NULL
			                            : epi.nonStructType.OutType),
			    epi.length, static_cast<USHORT>(pb_data_end - pb_data), const_cast<PBYTE>(pb_data),
			    &cbBuffer, m_propertyBuffer.data(), &cbUsed);

			if (status == ERROR_INSUFFICIENT_BUFFER && m_propertyBuffer.size() < cbBuffer / 2) {
				// Try again with a bigger buffer.
				m_propertyBuffer.resize(cbBuffer / 2);
				continue;
			} else {
				pb_data += cbUsed;
			}

			break;
		}

		// now that it is formatted, time to use
		if (prop_index == 0) { // pid
			std::string pid_char = to_string(m_propertyBuffer);
			std::from_chars(pid_char.c_str(), pid_char.c_str() + pid_char.length(),
			                network_packet.pid);
		} else if (prop_index == 1) { // size
			std::string size_str = to_string(m_propertyBuffer);
			std::from_chars(size_str.c_str(), size_str.c_str() + size_str.length(),
			                network_packet.size);
		} else if (prop_index == 2) { // dst_addr
			std::string dst_addr = to_string(m_propertyBuffer);
			network_packet.remote_addr = ip_string_to_uint32(dst_addr);
		} else if (prop_index == 3) { // src_addr
			std::string src_addr = to_string(m_propertyBuffer);
			network_packet.local_addr = ip_string_to_uint32(src_addr);
		} else if (prop_index == 4) { // dst_port
			std::string dst_port_str = to_string(m_propertyBuffer);
			std::from_chars(dst_port_str.c_str(), dst_port_str.c_str() + dst_port_str.length(),
			                network_packet.remote_port);
		} else if (prop_index == 5) { // src_port
			std::string src_port_str = to_string(m_propertyBuffer);
			std::from_chars(src_port_str.c_str(), src_port_str.c_str() + src_port_str.length(),
			                network_packet.local_port);
		}
	}
	return network_packet;
}

void NetworkETWHandler::setup_network() {
	std::thread t(start_trace);
	t.detach();
}

std::unordered_map<std::uint32_t, NetworkStat> NetworkETWHandler::get_pid_network_stat() {
	std::lock_guard<std::mutex> lock(pid_network_stat_map_mutex);
	auto ret = std::move(pid_network_stat_map);
	pid_network_stat_map.clear(); // just to be safe
	return ret;
}

#else

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pcap.h>
#include <unordered_map>

#include "client/process_stat.hpp"
#include <iostream>

PcapHandler::PcapHandler() { handle = NULL; }

void PcapHandler::setup_network() {
	char errbuf[PCAP_ERRBUF_SIZE];
	handle = pcap_open_live("ens33", PCAP_ERRBUF_SIZE, 1, 1000, errbuf);
	if (handle == NULL) {
		std::cerr << "Error opening device: " << errbuf << "\n";
	}
	if (pcap_setnonblock(handle, 1, errbuf) < 0) {
		std::cerr << "Error setting non-blocking mode: " << errbuf << "\n";
	}
}

PcapHandler::~PcapHandler() {
	if (handle != NULL) {
		pcap_close(handle);
	}
}

std::unordered_map<std::uint32_t, NetworkStat> PcapHandler::process_packets(
    const std::uint32_t local_ip,
    const std::unordered_map<std::uint16_t, std::uint32_t>& port_to_pid_map) {
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_map;
	struct pcap_pkthdr header;
	const u_char* packet;
	while (true) {
		packet = pcap_next(handle, &header);
		if (packet == NULL) {
			break;
		}
		struct ip* ip_hdr = (struct ip*)(packet + 14); // skip Ethernet

		if (ip_hdr->ip_p != IPPROTO_TCP) {
			continue;
		}

		struct tcphdr* tcp_hdr = (struct tcphdr*)(packet + 14 + ip_hdr->ip_hl * 4);

		int src_port = ntohs(tcp_hdr->source);
		int dst_port = ntohs(tcp_hdr->dest);
		// char src_ip[INET_ADDRSTRLEN];
		// char dst_ip[INET_ADDRSTRLEN];

		// inet_ntop(AF_INET, &(ip_hdr->ip_src), src_ip, INET_ADDRSTRLEN);
		// inet_ntop(AF_INET, &(ip_hdr->ip_dst), dst_ip, INET_ADDRSTRLEN);

		if (ip_hdr->ip_src.s_addr == local_ip) {
			// out going
			// std::cerr << src_port << "\n";
			// std::cerr << "out going: " << src_port << " ";
			// std::cerr << "at " << port_to_pid_map.at(src_port) << "\n";
			auto it = port_to_pid_map.find(src_port);
			if (it == port_to_pid_map.end()) {
				continue;
			}
			auto& network_usage = pid_network_map[it->second];
			std::uint64_t network_send = network_usage.network_send.value_or(0);
			network_usage.network_send = network_send + header.len;
		} else {
			// in going
			auto it = port_to_pid_map.find(dst_port);
			if (it == port_to_pid_map.end()) {
				continue;
			}
			auto& network_usage = pid_network_map[it->second];
			std::uint64_t network_recv = network_usage.network_recv.value_or(0);
			network_usage.network_recv = network_recv + header.len;
		}
	}
	return pid_network_map;
}

#endif
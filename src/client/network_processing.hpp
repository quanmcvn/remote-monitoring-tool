#ifndef CLIENT_NETWORK_PROCESSING_HPP
#define CLIENT_NETWORK_PROCESSING_HPP

#include "client/process_stat.hpp"

#include <cstdint>
#include <unordered_map>

#include "common/network.hpp"

#ifdef _WIN32

// we gonna use ETW to process network data
// code from
// https://learn.microsoft.com/en-us/windows/win32/etw/using-tdhformatproperty-to-consume-event-data

#include <windows.h>

#define INITGUID // Ensure that EventTraceGuid is defined.
#include <evntrace.h>
#undef INITGUID

#include <tdh.h>
#include <vector>

#include <wchar.h>

// #pragma comment(lib, "tdh.lib") // Link against TDH.dll

struct NetworkPacketInfo {
	std::uint32_t pid = 0;
	std::uint32_t size = 0;
	std::uint32_t local_addr = 0;
	std::uint16_t local_port = 0;
	std::uint32_t remote_addr = 0;
	std::uint16_t remote_port = 0;
	bool receive = 0;
};

class DecoderContext {
public:
	/*
	Initialize the decoder context.
	Sets up the TDH_CONTEXT array that will be used for decoding.
	*/
	explicit DecoderContext(_In_opt_ LPCWSTR szTmfSearchPath);

	// assume the event is a trace event
	// then assume that it's from provider Microsoft-Windows-Kernel-Network
	NetworkPacketInfo get_network_packet(_In_ PEVENT_RECORD pEventRecord);
private:
	/*
	Converts a TRACE_EVENT_INFO offset (e.g. TaskNameOffset) into a string.
	*/
	_Ret_z_ LPCWSTR TeiString(unsigned offset) {
		return reinterpret_cast<LPCWSTR>(m_teiBuffer.data() + offset);
	}

private:
	TDH_CONTEXT m_tdhContext[1]; // May contain TDH_CONTEXT_WPP_TMFSEARCHPATH.
	BYTE m_tdhContextCount;      // 1 if a TMF search path is present.
	std::vector<BYTE> m_teiBuffer;       // Buffer for TRACE_EVENT_INFO data.
};

class NetworkETWHandler {
public:
	void setup_network();
	// move out, effectively clearing it after each call
	std::unordered_map<std::uint32_t, NetworkStat> get_pid_network_stat();
private:
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_stat;
};

#else

#include <pcap/pcap.h>

class PcapHandler {
	pcap_t* handle;

public:
	PcapHandler();
	~PcapHandler();
	void setup_network();
	std::unordered_map<std::uint32_t, NetworkStat>
	process_packets(const std::uint32_t local_ip,
	                const std::unordered_map<std::uint16_t, std::uint32_t>& port_to_pid_map);
};

#endif

#endif
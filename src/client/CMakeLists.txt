add_library(client_lib)
target_sources(client_lib
	PRIVATE
		client_logger.cpp
		client.cpp
		log_queue.cpp
		network_processing.cpp
		process_listing.cpp
		process_table.cpp
		server_connector.cpp
)
target_include_directories(client_lib
	PUBLIC
		${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(client_lib
	PUBLIC
		common
)

add_executable(client)
target_sources(client
	PRIVATE
		main.cpp
)
target_link_libraries(client
	PUBLIC
		client_lib
)

if (WIN32)
	find_path(PCAP_INCLUDE_DIR
		pcap.h
		HINTS
			ENV NPCAP_DIR
			PATH_SUFFIXES include Include
		REQUIRED
	)

	find_library(WPCAP_LIBRARY
		NAMES wpcap
		HINTS
			ENV NPCAP_DIR
			PATH_SUFFIXES lib Lib/x64
		REQUIRED
	)

	find_library(PACKET_LIBRARY
		NAMES Packet
		HINTS
			ENV NPCAP_DIR
			PATH_SUFFIXES lib Lib/x64
		REQUIRED
	)

	message(STATUS "WPCAP_LIBRARY = ${WPCAP_LIBRARY}")
	message(STATUS "PACKET_LIBRARY = ${PACKET_LIBRARY}")

	target_include_directories(client_lib PUBLIC ${PCAP_INCLUDE_DIR})

	target_link_libraries(client_lib PUBLIC
		${WPCAP_LIBRARY}
		${PACKET_LIBRARY}
		ws2_32
		ole32
		oleaut32
		wbemuuid
		iphlpapi
		pdh
		psapi
	)
else()
	target_link_libraries(client_lib PUBLIC pcap)
endif()
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
	target_link_libraries(client_lib PUBLIC
		ws2_32
		ole32
		oleaut32
		wbemuuid
		iphlpapi
		psapi
		tdh
	)
else()
	target_link_libraries(client_lib PUBLIC pcap)
endif()
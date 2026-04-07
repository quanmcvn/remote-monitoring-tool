add_library(server_lib)
target_sources(server_lib
	PRIVATE
		client_handler.cpp
		client_manager.cpp
		server.cpp
)
target_include_directories(server_lib
	PUBLIC
		${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(server_lib
	PUBLIC
		common
)

add_executable(server)
target_sources(server
	PRIVATE
		main.cpp
)
target_link_libraries(server
	PUBLIC
		server_lib
)

if (WIN32)
	target_link_libraries(server_lib PUBLIC ws2_32)
endif()
add_library(common)

target_sources(common
	PRIVATE
		config_entry.cpp
		config.cpp
		event_bus.cpp
		exponential_backoff.cpp
		error.cpp
		input_event.cpp
		input_handler.cpp
		log_entry.cpp
		network_event.cpp
		network.cpp
		serializable.cpp
		util.cpp
)


if (WIN32)
target_sources(common
	PRIVATE
		registry.cpp
)
endif()

target_include_directories(common
	PUBLIC
		${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(common
	PUBLIC
		nlohmann_json::nlohmann_json
)
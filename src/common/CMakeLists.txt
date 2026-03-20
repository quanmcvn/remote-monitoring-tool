add_library(common)

target_sources(common
	PRIVATE
		config_entry.cpp
		config.cpp
		error.cpp
		log_entry.cpp
		network.cpp
		serializable.cpp
		util.cpp
)

target_include_directories(common
	PUBLIC
		${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(common
	PUBLIC
		nlohmann_json::nlohmann_json
)
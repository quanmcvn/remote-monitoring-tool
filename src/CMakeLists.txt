include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP)
FetchContent_MakeAvailable(json)

add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(server)

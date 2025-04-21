
add_executable(tcp_client tcp_client.cpp)
add_executable(tcp_server tcp_server.cpp)
add_executable(udp_client udp_client.cpp)

target_include_directories(tcp_client PRIVATE ${PROJECT_SOURCE_DIR}/inc)
target_include_directories(tcp_server PRIVATE ${PROJECT_SOURCE_DIR}/inc)
target_include_directories(udp_client PRIVATE ${PROJECT_SOURCE_DIR}/inc)

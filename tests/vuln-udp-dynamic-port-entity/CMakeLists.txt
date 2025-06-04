cmake_minimum_required(VERSION 3.10)
project(vuln_udp_dynamic_port_entity C)

# Build the “primary” server on port 5000
add_executable(server_primary
    server_primary.c
)
# Optionally, set C standard and warnings:
target_compile_features(server_primary PRIVATE c_std_99)
target_compile_options(server_primary PRIVATE -Wall -Wextra)

# Build the “dynamic” client that talks to the primary
add_executable(client_dynamic
    client_dynamic.c
)
target_compile_features(client_dynamic PRIVATE c_std_99)
target_compile_options(client_dynamic PRIVATE -Wall -Wextra)

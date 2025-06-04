cmake_minimum_required(VERSION 3.10)
project(vuln_udp_simple_file_transfer C)

# Build ftserver (listening on port 8000)
add_executable(ftserver
    ftserver.c
)
target_compile_features(ftserver PRIVATE c_std_99)
target_compile_options(ftserver PRIVATE -Wall -Wextra)

# Build the UDP file‐transfer client
add_executable(file_client
    file_client.c
)
target_compile_features(file_client PRIVATE c_std_99)
target_compile_options(file_client PRIVATE -Wall -Wextra)

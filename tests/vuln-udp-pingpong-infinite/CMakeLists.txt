cmake_minimum_required(VERSION 3.10)
project(vuln_udp_pingpong_infinite C)

# Build the ping/pong program (A or B mode via command line)
add_executable(pingpong
    pingpong.c
)
target_compile_features(pingpong PRIVATE c_std_99)
target_compile_options(pingpong PRIVATE -Wall -Wextra)

cmake_minimum_required(VERSION 3.10)
project(vuln_udp_master_slave_broadcast C)

# Build the master, listening on port 7000
add_executable(master_broadcast
    master.c
)
target_compile_features(master_broadcast PRIVATE c_std_99)
target_compile_options(master_broadcast PRIVATE -Wall -Wextra)

# Build each slave, listening on one of ports 7101/7102/7103
add_executable(slave_broadcast
    slave.c
)
target_compile_features(slave_broadcast PRIVATE c_std_99)
target_compile_options(slave_broadcast PRIVATE -Wall -Wextra)

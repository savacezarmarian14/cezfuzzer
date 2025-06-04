cmake_minimum_required(VERSION 3.10)
project(UDPTests)

# Add each subfolder under tests/ as its own CMake subdirectory.
# The folder names must match exactly (including any hyphens).
add_subdirectory(vuln-udp-dynamic-port-entity)
add_subdirectory(vuln-udp-master-slave-broadcast)
add_subdirectory(vuln-udp-pingpong-infinite)
add_subdirectory(vuln-udp-simple-file-transfer)
add_subdirectory(vuln-udp-stack-overflow)

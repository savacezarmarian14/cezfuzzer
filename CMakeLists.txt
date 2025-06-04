cmake_minimum_required(VERSION 3.16)
project(cezfuzzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the subdirectory containing the proxy application
add_subdirectory(proxy)
add_subdirectory(luncher)

# Add the tests suite 
add_subdirectory(tests)

# Optionally add commander/ if it becomes a C++ component

# add_subdirectory(commander)




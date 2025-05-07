# Collect all C++ source and header files from proxy/src and proxy/inc
file(GLOB_RECURSE SERVER_SRC src/*.cpp)
file(GLOB_RECURSE SERVER_HEADERS inc/*.hpp)

find_package(yaml-cpp REQUIRED)

# Build the main MITM proxy application as an executable
add_executable(server ${SERVER_SRC} ${SERVER_HEADERS})

# Include the header files from inc/ directory
target_include_directories(server PUBLIC inc)
target_include_directories(server PUBLIC lib)

# Link against pthread for multithreading support
target_link_libraries(server pthread)
# Link against libyaml-cpp
target_link_libraries(server yaml-cpp)


# Collect all C++ source and header files from proxy/src and proxy/inc
file(GLOB_RECURSE CLIENT_SRC src/*.cpp)
file(GLOB_RECURSE CLIENT_HEADERS inc/*.hpp)

# Build the main MITM proxy application as an executable
add_executable(client ${CLIENT_SRC} ${CLIENT_HEADERS})

# Include the header files from inc/ directory
target_include_directories(client PUBLIC inc)
target_include_directories(client PUBLIC lib)

# Link against pthread for multithreading support
target_link_libraries(client pthread)

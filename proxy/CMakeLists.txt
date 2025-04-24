# Collect all C++ source and header files from proxy/src and proxy/inc
file(GLOB_RECURSE PROXY_SRC src/*.cpp)
file(GLOB_RECURSE PROXY_HEADERS inc/*.hpp)

# Build the main MITM proxy application as an executable
add_executable(proxy_fuzzer ${PROXY_SRC} ${PROXY_HEADERS})

# Include the header files from inc/ directory
target_include_directories(proxy_fuzzer PUBLIC inc)

# Link against pthread for multithreading support
target_link_libraries(proxy_fuzzer pthread)

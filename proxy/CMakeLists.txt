file(GLOB_RECURSE PROXY_SRC src/*.cpp)
file(GLOB_RECURSE PROXY_HEADERS inc/*.hpp)
file(GLOB_RECURSE PROXY_SRC src/*.cpp lib/*.cpp)
file(GLOB_RECURSE PROXY_HEADERS inc/*.hpp lib/*.hpp)

# Executabilul principal
add_executable(proxy_fuzzer ${PROXY_SRC} ${PROXY_HEADERS})

# Include directories
target_include_directories(proxy_fuzzer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)



# Thread support
target_link_libraries(proxy_fuzzer 
    pthread
    yaml-cpp                    
)

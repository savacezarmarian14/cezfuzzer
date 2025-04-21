# Build the test1-fuzzer executable from test1-fuzzer.cpp

add_executable(test1-fuzzer test1-fuzzer.cpp)

# Include the inc/ directory for shared headers (optional, if you use custom .hpp files)
target_include_directories(test1-fuzzer PRIVATE ${PROJECT_SOURCE_DIR}/inc)

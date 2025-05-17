add_library(utils
    ConfigurationManager.cpp
    ExecutionManager.cpp
)

target_include_directories(utils
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(utils
    PUBLIC yaml-cpp                    
)
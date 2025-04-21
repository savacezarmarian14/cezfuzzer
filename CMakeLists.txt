cmake_minimum_required(VERSION 3.10)
project(cezfuzzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(test_src)

# Dacă vrei să adaugi mai târziu logică principală:
add_subdirectory(src)

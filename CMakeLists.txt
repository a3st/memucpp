cmake_minimum_required(VERSION 3.2.4)
project(memucpp VERSION 0.1.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

include_directories(
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include"
)

add_executable(memucpp-test tests/main.cpp)
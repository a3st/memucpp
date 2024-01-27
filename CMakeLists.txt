cmake_minimum_required(VERSION 3.26.0)
project(memucpp VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

add_library(memucpp STATIC
    "src/memucpp.cpp"
)

target_include_directories(memucpp PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/thirdparty/stb/include"
)

add_executable(memucpp-test tests/main.cpp)

target_link_libraries(memucpp-test PRIVATE memucpp)
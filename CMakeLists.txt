# Copyright © 2020-2024 Dmitriy Lukovenko. All rights reserved.

cmake_minimum_required(VERSION 3.25.1)

project(memucpp 
    VERSION 1.0.0 
    DESCRIPTION "MEmu command wrapper library"
    HOMEPAGE_URL https://github.com/a3st/memucpp
    LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

option(BUILD_TESTING "Build memucpp tests" TRUE)

add_library(memucpp STATIC src/memucpp.cpp)

target_include_directories(memucpp PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include)

if(BUILD_TESTING)
    add_executable(image_test tests/image_test.cpp)

    target_link_libraries(image_test PRIVATE memucpp)
endif()
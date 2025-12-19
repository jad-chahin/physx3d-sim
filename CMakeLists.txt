cmake_minimum_required(VERSION 3.20)
project(physics3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(physics3d
        src/main.cpp
)

if (MSVC)
    target_compile_options(physics3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(physics3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

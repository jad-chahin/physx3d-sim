cmake_minimum_required(VERSION 3.20)
project(physics3d LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

add_executable(physics3d
        src/main.cpp
        src/sim/Vec3.h
        src/sim/Body.h
        src/sim/World.cpp
        src/sim/World.h
        src/third_party/glad/src/glad.c
)

target_include_directories(physics3d PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/glad/include
)

target_link_libraries(physics3d PRIVATE glfw opengl32 glm::glm)

if (MSVC)
    target_compile_options(physics3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(physics3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

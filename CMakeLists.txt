cmake_minimum_required(VERSION 3.20)
project(physics3d LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # CLion's bundled MinGW keeps cc1/cc1plus under libexec and runtime DLLs under bin.
    # Ensure compile steps can resolve those DLLs even when PATH is minimal.
    get_filename_component(PHYSICS3D_GNU_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if (EXISTS "${PHYSICS3D_GNU_BIN_DIR}/libwinpthread-1.dll")
        string(REPLACE ";" "\\;" PHYSICS3D_TOOLCHAIN_PATH
                "${PHYSICS3D_GNU_BIN_DIR};$ENV{PATH}")
        set(PHYSICS3D_GNU_LAUNCHER
                "${CMAKE_COMMAND};-E;env;PATH=${PHYSICS3D_TOOLCHAIN_PATH}")
        set(CMAKE_C_COMPILER_LAUNCHER "${PHYSICS3D_GNU_LAUNCHER}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${PHYSICS3D_GNU_LAUNCHER}")
        set(CMAKE_C_LINKER_LAUNCHER "${PHYSICS3D_GNU_LAUNCHER}")
        set(CMAKE_CXX_LINKER_LAUNCHER "${PHYSICS3D_GNU_LAUNCHER}")
    endif()
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(PHYSICS3D_GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
set(PHYSICS3D_GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")

if (NOT EXISTS "${PHYSICS3D_GLFW_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
            "Missing vendored GLFW at ${PHYSICS3D_GLFW_DIR}. "
            "Initialize/populate third_party/glfw before configuring.")
endif()
if (NOT EXISTS "${PHYSICS3D_GLM_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
            "Missing vendored GLM at ${PHYSICS3D_GLM_DIR}. "
            "Initialize/populate third_party/glm before configuring.")
endif()

add_subdirectory("${PHYSICS3D_GLFW_DIR}" "${CMAKE_BINARY_DIR}/third_party/glfw")
add_subdirectory("${PHYSICS3D_GLM_DIR}" "${CMAKE_BINARY_DIR}/third_party/glm")

add_executable(physics3d
        src/main.cpp
        src/app/AppLoop.cpp
        src/sim/DefaultWorld.cpp
        src/sim/World.cpp
        src/sim/Broadphase.cpp
        src/sim/Collision.cpp
        src/third_party/glad/src/glad.c
        src/ui/OverlayRenderer.cpp
        src/ui/PauseMenuController.cpp
        src/render/ShaderUtil.cpp
        src/render/SphereMesh.cpp
        src/input/Camera.cpp
        src/input/Bindings.cpp
)

set(PHYSICS3D_GLAD_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/glad/src/glad.c")

target_include_directories(physics3d PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/glad/include
)

if (TARGET glm::glm)
    target_link_libraries(physics3d PRIVATE glfw opengl32 glm::glm)
else()
    target_link_libraries(physics3d PRIVATE glfw opengl32 glm)
endif()

if (MSVC)
    target_compile_options(physics3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(physics3d PRIVATE -Wall -Wextra -Wpedantic)
    set_source_files_properties("${PHYSICS3D_GLAD_SOURCE}" PROPERTIES
            COMPILE_OPTIONS "-Wno-pedantic")
endif()

cmake_minimum_required(VERSION 3.20)
project(physics3d LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
        src/sim/DefaultScene.cpp
        src/sim/World.cpp
        src/sim/Broadphase.cpp
        src/sim/Collision.cpp
        src/third_party/glad/src/glad.c
        src/ui/DebugOverlay.cpp
        src/ui/PauseMenuController.cpp
        src/render/ShaderUtil.cpp
        src/render/SphereMesh.cpp
        src/input/Camera.cpp
        src/input/Bindings.cpp
)

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
endif()

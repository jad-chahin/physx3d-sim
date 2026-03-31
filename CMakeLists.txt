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

set(PHYSICS3D_GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(PHYSICS3D_GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
set(PHYSICS3D_GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")

if (NOT EXISTS "${PHYSICS3D_GLAD_DIR}/src/glad.c")
    message(FATAL_ERROR
            "Missing vendored GLAD at ${PHYSICS3D_GLAD_DIR}. "
            "Initialize/populate third_party/glad before configuring.")
endif()
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

add_library(physics3d_glad STATIC)
target_sources(physics3d_glad PRIVATE
        third_party/glad/src/glad.c
)
target_include_directories(physics3d_glad PUBLIC
        ${PHYSICS3D_GLAD_DIR}/include
)

add_library(physics3d_sim STATIC)
target_sources(physics3d_sim PRIVATE
        src/sim/DefaultWorld.cpp
        src/sim/Material.cpp
        src/sim/World.cpp
        src/sim/WorldCollision.cpp
        src/sim/Broadphase.cpp
        src/sim/Collision.cpp
)
target_include_directories(physics3d_sim PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(physics3d
        src/main.cpp
)
target_sources(physics3d PRIVATE
        src/app/AppLoop.cpp
        src/app/AppBootstrap.cpp
        src/app/InputStage.cpp
        src/app/RenderStage.cpp
        src/app/SimulationStage.cpp
        src/app/CameraFocus.cpp
        src/app/AppLoopSupport.cpp
        src/app/FixedStepRunner.cpp
        src/app/SceneSnapshot.cpp
        src/app/ScenePresentation.cpp
        src/app/SpatialHud.cpp
)
target_sources(physics3d PRIVATE
        src/ui/OverlayRenderer.cpp
        src/ui/OverlayRendererCore.cpp
        src/ui/PauseMenu.cpp
        src/ui/PauseMenuActions.cpp
        src/ui/PauseMenuInput.cpp
        src/ui/PauseMenuLayout.cpp
        src/ui/PauseMenuNavigation.cpp
        src/ui/PauseMenuView.cpp
)
target_sources(physics3d PRIVATE
        src/render/FrameRenderer.cpp
        src/render/FrameRendererPasses.cpp
        src/render/FrameRendererResources.cpp
        src/render/RenderMaterial.cpp
        src/render/SceneLighting.cpp
        src/render/SceneRenderer.cpp
        src/render/ShaderUtil.cpp
        src/render/SphereMesh.cpp
)
target_sources(physics3d PRIVATE
        src/input/Camera.cpp
        src/input/Bindings.cpp
)
target_include_directories(physics3d PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

enable_testing()

add_executable(physics3d_tests)
target_sources(physics3d_tests PRIVATE
        tests/TestMain.cpp
        tests/PhysicsCoreTests.cpp
        tests/FocusCameraTests.cpp
        tests/MinimapTests.cpp
        tests/RenderMaterialTests.cpp
        tests/SceneLightingTests.cpp
        tests/UiPauseMenuTests.cpp
)
target_sources(physics3d_tests PRIVATE
        src/app/AppLoopSupport.cpp
        src/app/CameraFocus.cpp
        src/app/ScenePresentation.cpp
        src/app/SpatialHud.cpp
)
target_sources(physics3d_tests PRIVATE
        src/input/Camera.cpp
        src/input/Bindings.cpp
)
target_sources(physics3d_tests PRIVATE
        src/render/RenderMaterial.cpp
        src/render/SceneRenderer.cpp
        src/render/SceneLighting.cpp
)
target_sources(physics3d_tests PRIVATE
        src/ui/PauseMenu.cpp
        src/ui/PauseMenuActions.cpp
        src/ui/PauseMenuInput.cpp
        src/ui/PauseMenuLayout.cpp
        src/ui/PauseMenuNavigation.cpp
        src/ui/PauseMenuView.cpp
)
target_include_directories(physics3d_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (TARGET glm::glm)
    target_link_libraries(physics3d PRIVATE physics3d_sim physics3d_glad glfw opengl32 glm::glm)
    target_link_libraries(physics3d_tests PRIVATE physics3d_sim physics3d_glad glfw opengl32 glm::glm)
else()
    target_link_libraries(physics3d PRIVATE physics3d_sim physics3d_glad glfw opengl32 glm)
    target_link_libraries(physics3d_tests PRIVATE physics3d_sim physics3d_glad glfw opengl32 glm)
endif()

if (MSVC)
    target_compile_options(physics3d PRIVATE /W4 /permissive-)
    target_compile_options(physics3d_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(physics3d_glad PRIVATE -Wno-pedantic)
    target_compile_options(physics3d_sim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(physics3d PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(physics3d_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND EXISTS "${PHYSICS3D_GNU_BIN_DIR}/libwinpthread-1.dll")
    add_test(
            NAME physics3d_tests
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${PHYSICS3D_TOOLCHAIN_PATH}" $<TARGET_FILE:physics3d_tests>)
else()
    add_test(NAME physics3d_tests COMMAND $<TARGET_FILE:physics3d_tests>)
endif()

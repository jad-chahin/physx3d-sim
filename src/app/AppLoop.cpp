#include "app/AppLoop.h"
#include "app/AppBootstrap.h"
#include "app/InputStage.h"
#include "app/RenderStage.h"
#include "app/SimulationController.h"
#include "app/SimulationStage.h"
#include "app/Windowing.h"

#include <iostream>
#include <string>

#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/FrameRenderer.h"
#include "ui/PauseMenu.h"

namespace app {

int runApp(sim::World world) {
    app_loop::GlfwSession glfwSession;
    if (!glfwSession.init()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    app_loop::WindowHandle window;
    window.reset(glfwCreateWindow(1920, 1080, "physics3d", nullptr, nullptr));
    if (window.get() == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        return 1;
    }

    app_loop::AppLoopState appState{};
    app_loop::configureWindow(window, appState);

    input::Camera cam{};
    app_loop::SimulationController simulation(std::move(world));
    app_loop::RuntimeState runtime{};
    input::ControlBindings controls{};
    const std::string controlsConfigPath = "controls.cfg";
    input::loadControlBindings(controls, controlsConfigPath);

    ui::PauseMenu pauseMenu;
    pauseMenu.loadSettings("settings.cfg");
    pauseMenu.applyCurrentDisplaySettings(window);

    if (!app_loop::initGlad()) {
        std::cerr << "Failed to initialize GLAD\n";
        return 1;
    }

    app_loop::printGlInfo();
    app_loop::configureOpenGl();

    render_scene::FrameRenderer frameRenderer;
    if (!frameRenderer.init()) {
        return 1;
    }
    runtime.simulation.fixedStep.lastFrameTime = glfwGetTime();
    app_loop::FrameRenderState frameRenderState{};

    while (!glfwWindowShouldClose(window)) {
        app_loop::processInputStage(
            window,
            appState,
            runtime,
            controls,
            pauseMenu,
            simulation,
            controlsConfigPath);
        app_loop::runSimulationStage(window, appState, simulation, controls, pauseMenu, runtime, cam);
        app_loop::renderStage(
            window,
            appState,
            runtime,
            pauseMenu,
            controls,
            simulation,
            cam,
            frameRenderer,
            frameRenderState);
    }

    frameRenderer.shutdown();
    return 0;
}

} // namespace app


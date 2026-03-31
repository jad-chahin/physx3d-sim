#include "app/AppLoop.h"
#include "app/AppLoopInternal.h"
#include "app/ScenePresentation.h"
#include "app/SpatialHud.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/FrameRenderer.h"
#include "render/ShaderUtil.h"
#include "sim/DefaultWorld.h"
#include "ui/PauseMenu.h"

namespace {

void registerCallbacks(GLFWwindow* window, app_loop::AppLoopState& appState) {
    glfwSetWindowUserPointer(window, &appState);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* callbackWindow, const int w, const int h) {
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr) {
            state->framebufferSize.w = w;
            state->framebufferSize.h = h;
        }
    });

    glfwSetKeyCallback(window, [](GLFWwindow* callbackWindow, const int key, const int scancode, const int action, const int mods) {
        (void)scancode;
        (void)mods;
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr && action == GLFW_PRESS && key != GLFW_KEY_UNKNOWN) {
            state->lastPressedKey = key;
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* callbackWindow, const double xoffset, const double yoffset) {
        (void)xoffset;
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr) {
            state->scrollDeltaY += yoffset;
        }
    });
}

void configureWindow(GLFWwindow* window, app_loop::AppLoopState& appState) {
    constexpr int kMinWindowWidth = 960;
    constexpr int kMinWindowHeight = 540;

    glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwGetFramebufferSize(window, &appState.framebufferSize.w, &appState.framebufferSize.h);
    registerCallbacks(window, appState);
}

bool initGlad() {
    return gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
}

void configureOpenGl() {
    int flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugCallback, nullptr);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

struct FrameRenderState {
    ui::MenuView menuView{};
    OverlayRenderer::SpatialHud spatialHud{};
    OverlayRenderer::FocusMarker focusMarker{};
    OverlayRenderer::FocusHint focusHint{};
    OverlayRenderer::TargetHud targetHud{};
    app_loop::SpatialHudScratch spatialHudScratch{};
    render_scene::SceneSnapshot sceneSnapshot{};
    std::uint64_t sceneSnapshotRevisionCounter = 0;
    std::uint64_t cachedStaticSceneRevision = 0;
    bool cachedStaticScene = false;
};

void printGlInfo() {
    std::cout << "Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION) << "\n";
}

void updatePauseMenu(
    GLFWwindow* window,
    app_loop::AppLoopState& appState,
    app_loop::RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenu& pauseMenu,
    app_loop::SimulationController& simulation,
    const std::string& controlsConfigPath)
{
    const int pressedKey = app_loop::consumeLastPressedKey(appState);
    const auto scrollDeltaY = static_cast<float>(app_loop::consumeScrollDeltaY(appState));

    pauseMenu.updateEscapeState(
        window,
        runtime.input.mouseCaptured,
        runtime.input.firstMouse,
        runtime.simulation.fixedStep.lastFrameTime,
        runtime.simulation.fixedStep.accumulator,
        runtime.simulation.fixedStep.alpha,
        simulation.mutableBodies());
    pauseMenu.handlePointerInput(window, controls, controlsConfigPath, scrollDeltaY);
    pauseMenu.handlePressedKey(window, pressedKey, controls, controlsConfigPath);
    pauseMenu.updateContinuousInput(window, controls);
}

void updateFrameState(
    GLFWwindow* window,
    app_loop::SimulationController& simulation,
    const input::ControlBindings& controls,
    const ui::PauseMenu& pauseMenu,
    app_loop::RuntimeState& runtime,
    input::Camera& cam,
    const render_scene::FramebufferSize& framebufferSize)
{
    const auto& simSettings = pauseMenu.simulationSettings();
    const auto& cameraSettings = pauseMenu.cameraSettings();

    simulation.applySettings(simSettings);
    runtime.simulation.simSpeed = std::clamp(runtime.simulation.simSpeed, simSettings.minSimSpeed, simSettings.maxSimSpeed);
    simulation.handleHotkeys(window, controls, simSettings, pauseMenu.isOpen(), runtime);

    const double now = glfwGetTime();
    const double frameTime = std::max(0.0, now - runtime.simulation.fixedStep.lastFrameTime);
    const double cameraFrameTime = std::min(frameTime, app_loop::kMaxCameraFrameTime);
    runtime.simulation.fixedStep.lastFrameTime = now;

    app_loop::updateFps(runtime, frameTime);
    const bool focusDown = input::isBindingPressed(window, controls.focusTarget);
    bool focusStartedThisFrame = false;
    if (!pauseMenu.isOpen() && focusDown && !runtime.focus.focusWasDown) {
        if (runtime.focus.camera.active) {
            app_loop::clearCameraFocus(runtime.focus.camera);
        } else if (simulation.hasBodies()) {
            render_scene::SceneSnapshot interactionSnapshot{};
            app_loop::buildSceneSnapshot(simulation, runtime, runtime.simulation.fixedStep.alpha, interactionSnapshot);
            const render_scene::SceneView interactionView =
                render_scene::buildSceneView(cam, cameraSettings, framebufferSize);
            const int targetIndex =
                app_loop::findTargetBodyIndex(interactionSnapshot, cam, interactionView);
            focusStartedThisFrame =
                app_loop::beginCameraFocus(interactionSnapshot, targetIndex, cam, runtime.focus.camera);
        }
    }
    runtime.focus.focusWasDown = focusDown;

    if (runtime.focus.camera.active &&
        app_loop::movementCanExitCameraFocus(pauseMenu.isOpen(), focusStartedThisFrame) &&
        app_loop::cameraTranslationPressed(window, controls))
    {
        app_loop::clearCameraFocus(runtime.focus.camera);
    }

    if (runtime.focus.camera.active && runtime.input.mouseCaptured) {
        app_loop::updateCameraLook(window, cameraSettings, runtime, cam);
    } else if (!runtime.focus.camera.active) {
        app_loop::updateCamera(window, controls, cameraSettings, runtime, cameraFrameTime, cam);
    }
    simulation.step(runtime, pauseMenu.isOpen(), frameTime);

    if (runtime.focus.camera.active && runtime.input.mouseCaptured) {
        render_scene::SceneSnapshot focusSnapshot{};
        app_loop::buildSceneSnapshot(simulation, runtime, runtime.simulation.fixedStep.alpha, focusSnapshot);
        app_loop::updateCameraFocus(
            focusSnapshot,
            static_cast<float>(cameraFrameTime),
            runtime.simulation.simSpeed,
            cam,
            runtime.focus.camera);
    }
}

void processInputStage(
    GLFWwindow* window,
    app_loop::AppLoopState& appState,
    app_loop::RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenu& pauseMenu,
    app_loop::SimulationController& simulation,
    const std::string& controlsConfigPath)
{
    glfwPollEvents();

    updatePauseMenu(window, appState, runtime, controls, pauseMenu, simulation, controlsConfigPath);
    if (pauseMenu.consumeResetWorldRequest()) {
        simulation.reset(runtime, pauseMenu.simulationSettings());
    }
}

void runSimulationStage(
    GLFWwindow* window,
    const app_loop::AppLoopState& appState,
    app_loop::SimulationController& simulation,
    const input::ControlBindings& controls,
    const ui::PauseMenu& pauseMenu,
    app_loop::RuntimeState& runtime,
    input::Camera& cam)
{
    updateFrameState(window, simulation, controls, pauseMenu, runtime, cam, appState.framebufferSize);
    app_loop::updatePathHistory(simulation, runtime, pauseMenu.interfaceSettings());
}

void renderStage(
    GLFWwindow* window,
    const app_loop::AppLoopState& appState,
    const app_loop::RuntimeState& runtime,
    const ui::PauseMenu& pauseMenu,
    const input::ControlBindings& controls,
    const app_loop::SimulationController& simulation,
    const input::Camera& cam,
    render_scene::FrameRenderer& frameRenderer,
    FrameRenderState& frameRenderState)
{
    frameRenderState.menuView = pauseMenu.buildView(controls);
    frameRenderState.spatialHud = {};
    frameRenderState.focusMarker = {};
    frameRenderState.focusHint = {};
    frameRenderState.targetHud = {};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, pauseMenu.cameraSettings(), appState.framebufferSize);
    const ui::InterfaceSettings& interfaceSettings = pauseMenu.interfaceSettings();
    const bool staticScene = runtime.simulation.simFrozen || pauseMenu.isOpen();

    if (simulation.hasBodies()) {
        const bool canReuseStaticSnapshot =
            staticScene &&
            frameRenderState.cachedStaticScene &&
            frameRenderState.cachedStaticSceneRevision == runtime.simulation.sceneRevision &&
            frameRenderState.sceneSnapshot.bodies.size() == simulation.bodies().size();
        if (!canReuseStaticSnapshot) {
            app_loop::buildSceneSnapshot(
                simulation,
                runtime,
                runtime.simulation.fixedStep.alpha,
                frameRenderState.sceneSnapshot);
            frameRenderState.sceneSnapshot.instanceRevision = ++frameRenderState.sceneSnapshotRevisionCounter;
        }
        frameRenderState.cachedStaticScene = staticScene;
        frameRenderState.cachedStaticSceneRevision = runtime.simulation.sceneRevision;
        frameRenderState.sceneSnapshot.pathTrails = runtime.pathHistory;
        frameRenderState.sceneSnapshot.pathTrailsRevision = runtime.pathHistoryRevision;
    } else {
        frameRenderState.sceneSnapshot.bodies.clear();
        frameRenderState.sceneSnapshot.models.clear();
        frameRenderState.sceneSnapshot.pathTrails = runtime.pathHistory;
        frameRenderState.sceneSnapshot.instanceRevision = ++frameRenderState.sceneSnapshotRevisionCounter;
        frameRenderState.sceneSnapshot.pathTrailsRevision = runtime.pathHistoryRevision;
        frameRenderState.cachedStaticScene = staticScene;
        frameRenderState.cachedStaticSceneRevision = runtime.simulation.sceneRevision;
    }
    const int focusedIndex =
        app_loop::findFocusedBodyIndex(frameRenderState.sceneSnapshot, runtime.focus.camera);
    const bool needsHoverTargetIndex = focusedIndex < 0 &&
        (interfaceSettings.objectInfo || interfaceSettings.showMinimap) &&
        !frameRenderState.sceneSnapshot.bodies.empty();
    const int targetIndex = focusedIndex >= 0
        ? focusedIndex
        : (needsHoverTargetIndex
            ? app_loop::findTargetBodyIndex(frameRenderState.sceneSnapshot, cam, sceneView)
            : -1);
    app_loop::buildFocusMarker(
        frameRenderState.sceneSnapshot,
        sceneView,
        appState.framebufferSize,
        focusedIndex,
        frameRenderState.focusMarker);
    app_loop::buildTargetHud(
        frameRenderState.sceneSnapshot,
        interfaceSettings,
        sceneView,
        appState.framebufferSize,
        targetIndex,
        frameRenderState.targetHud);
    app_loop::buildSpatialHud(
        frameRenderState.sceneSnapshot,
        interfaceSettings,
        cam,
        sceneView,
        targetIndex,
        frameRenderState.spatialHudScratch,
        frameRenderState.spatialHud);
    if (runtime.focus.camera.active) {
        frameRenderState.focusHint.visible = true;
        frameRenderState.focusHint.label =
            input::keyNameForCode(controls.focusTarget) + std::string{" OR MOVE TO EXIT FOCUS"};
    }

    const render_scene::OverlayPassInput overlayInput{
        &frameRenderState.menuView,
        &frameRenderState.spatialHud,
        &frameRenderState.focusMarker,
        &frameRenderState.focusHint,
        &frameRenderState.targetHud,
        pauseMenu.uiScale(),
        &interfaceSettings,
    };
    const render_scene::FrameInput frameInput{
        appState.framebufferSize,
        runtime.simulation.simFrozen,
        runtime.simulation.simSpeed,
        runtime.simulation.elapsedTime,
        runtime.fps.displayed,
        interfaceSettings.drawPath,
        interfaceSettings.pathColorIndex,
        &sceneView,
        &frameRenderState.sceneSnapshot,
        &overlayInput,
    };
    frameRenderer.render(frameInput);

    glfwSwapBuffers(window);
}

} // namespace

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
    configureWindow(window, appState);

    input::Camera cam{};
    app_loop::SimulationController simulation(std::move(world));
    app_loop::RuntimeState runtime{};
    input::ControlBindings controls{};
    const std::string controlsConfigPath = "controls.cfg";
    input::loadControlBindings(controls, controlsConfigPath);

    ui::PauseMenu pauseMenu;
    pauseMenu.loadSettings("settings.cfg");
    pauseMenu.applyCurrentDisplaySettings(window);

    if (!initGlad()) {
        std::cerr << "Failed to initialize GLAD\n";
        return 1;
    }

    printGlInfo();
    configureOpenGl();

    render_scene::FrameRenderer frameRenderer;
    if (!frameRenderer.init()) {
        return 1;
    }
    runtime.simulation.fixedStep.lastFrameTime = glfwGetTime();
    FrameRenderState frameRenderState{};

    while (!glfwWindowShouldClose(window)) {
        processInputStage(
            window,
            appState,
            runtime,
            controls,
            pauseMenu,
            simulation,
            controlsConfigPath);
        runSimulationStage(window, appState, simulation, controls, pauseMenu, runtime, cam);
        renderStage(
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


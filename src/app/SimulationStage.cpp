#include "app/SimulationStage.h"

#include <algorithm>

#include "app/CameraFocus.h"
#include "app/FrameUpdate.h"
#include "app/ScenePresentation.h"
#include "app/SceneSnapshot.h"

namespace {

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
                app_loop::beginCameraFocus(
                    interactionSnapshot,
                    targetIndex,
                    cam,
                    cameraSettings.fovDegrees,
                    runtime.focus.camera);
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
            runtime.input.scrollDeltaY,
            cameraSettings.fovDegrees,
            cam,
            runtime.focus.camera);
    }
    runtime.input.scrollDeltaY = 0.0f;
}

} // namespace

namespace app_loop {

void runSimulationStage(
    GLFWwindow* window,
    const AppLoopState& appState,
    SimulationController& simulation,
    const input::ControlBindings& controls,
    const ui::PauseMenu& pauseMenu,
    RuntimeState& runtime,
    input::Camera& cam)
{
    updateFrameState(window, simulation, controls, pauseMenu, runtime, cam, appState.framebufferSize);
    updatePathHistory(simulation, runtime, pauseMenu.interfaceSettings());
}

} // namespace app_loop

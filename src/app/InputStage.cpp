#include "app/InputStage.h"

#include "app/Windowing.h"

namespace {

void updatePauseMenu(
    GLFWwindow* window,
    app_loop::AppLoopState& appState,
    app_loop::RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenu& pauseMenu,
    app_loop::SimulationController& simulation,
    const std::string& controlsConfigPath,
    const float scrollDeltaY)
{
    const int pressedKey = app_loop::consumeLastPressedKey(appState);

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

} // namespace

namespace app_loop {

void processInputStage(
    GLFWwindow* window,
    AppLoopState& appState,
    RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenu& pauseMenu,
    SimulationController& simulation,
    const std::string& controlsConfigPath)
{
    glfwPollEvents();

    const float scrollDeltaY = static_cast<float>(consumeScrollDeltaY(appState));
    runtime.input.scrollDeltaY = scrollDeltaY;
    updatePauseMenu(window, appState, runtime, controls, pauseMenu, simulation, controlsConfigPath, scrollDeltaY);
    if (pauseMenu.consumeResetWorldRequest()) {
        simulation.reset(runtime, pauseMenu.simulationSettings());
    }
}

} // namespace app_loop

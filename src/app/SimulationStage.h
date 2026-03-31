#ifndef PHYSICS3D_APP_SIMULATIONSTAGE_H
#define PHYSICS3D_APP_SIMULATIONSTAGE_H

#include "app/AppRuntime.h"
#include "app/SimulationController.h"
#include "input/Bindings.h"
#include "input/Camera.h"
#include "ui/PauseMenu.h"

struct GLFWwindow;

namespace app_loop {

void runSimulationStage(
    GLFWwindow* window,
    const AppLoopState& appState,
    SimulationController& simulation,
    const input::ControlBindings& controls,
    const ui::PauseMenu& pauseMenu,
    RuntimeState& runtime,
    input::Camera& cam);

} // namespace app_loop

#endif // PHYSICS3D_APP_SIMULATIONSTAGE_H

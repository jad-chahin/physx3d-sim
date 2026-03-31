#ifndef PHYSICS3D_APP_INPUTSTAGE_H
#define PHYSICS3D_APP_INPUTSTAGE_H

#include <string>

#include "app/AppRuntime.h"
#include "app/SimulationController.h"
#include "input/Bindings.h"
#include "ui/PauseMenu.h"

struct GLFWwindow;

namespace app_loop {

void processInputStage(
    GLFWwindow* window,
    AppLoopState& appState,
    RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenu& pauseMenu,
    SimulationController& simulation,
    const std::string& controlsConfigPath);

} // namespace app_loop

#endif // PHYSICS3D_APP_INPUTSTAGE_H

#ifndef PHYSICS3D_APP_FRAMEUPDATE_H
#define PHYSICS3D_APP_FRAMEUPDATE_H

#include "app/AppRuntime.h"
#include "input/Bindings.h"
#include "input/Camera.h"
#include "ui/PauseMenu.h"

struct GLFWwindow;

namespace app_loop {

void updateFps(RuntimeState& runtime, double frameTime);

void updateCamera(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    double frameTime,
    input::Camera& cam);

void updateCameraLook(
    GLFWwindow* window,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    input::Camera& cam);

[[nodiscard]] bool cameraTranslationPressed(
    GLFWwindow* window,
    const input::ControlBindings& controls);

[[nodiscard]] bool movementCanExitCameraFocus(
    bool pauseMenuOpen,
    bool focusStartedThisFrame);

} // namespace app_loop

#endif // PHYSICS3D_APP_FRAMEUPDATE_H

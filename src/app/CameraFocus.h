#ifndef PHYSICS3D_APP_CAMERAFOCUS_H
#define PHYSICS3D_APP_CAMERAFOCUS_H

#include <cstdint>

#include "input/Camera.h"
#include "render/SceneRenderer.h"

namespace app_loop {

struct CameraFocusState {
    bool active = false;
    std::uint64_t bodyId = 0;
    float followDistance = 0.0f;
};

[[nodiscard]] int findFocusedBodyIndex(
    const render_scene::SceneSnapshot& snapshot,
    const CameraFocusState& focusState);

void clearCameraFocus(CameraFocusState& focusState);

bool beginCameraFocus(
    const render_scene::SceneSnapshot& snapshot,
    int targetBodyIndex,
    input::Camera& cam,
    float fovDegrees,
    CameraFocusState& focusState);

bool updateCameraFocus(
    const render_scene::SceneSnapshot& snapshot,
    float scrollDeltaY,
    float fovDegrees,
    input::Camera& cam,
    CameraFocusState& focusState);

} // namespace app_loop

#endif // PHYSICS3D_APP_CAMERAFOCUS_H

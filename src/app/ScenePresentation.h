#ifndef PHYSICS3D_APP_SCENEPRESENTATION_H
#define PHYSICS3D_APP_SCENEPRESENTATION_H

#include <vector>

#include "input/Camera.h"
#include "render/SceneRenderer.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

namespace app_loop {

struct RuntimeState;
class SimulationController;

[[nodiscard]] int findTargetBodyIndex(
    const render_scene::SceneSnapshot& snapshot,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView);

void updatePathHistory(
    const SimulationController& simulation,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings);

void buildTargetHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    int targetBodyIndex,
    OverlayRenderer::TargetHud& targetHud);

void buildFocusMarker(
    const render_scene::SceneSnapshot& snapshot,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    int focusedBodyIndex,
    OverlayRenderer::FocusMarker& focusMarker);

} // namespace app_loop

#endif // PHYSICS3D_APP_SCENEPRESENTATION_H

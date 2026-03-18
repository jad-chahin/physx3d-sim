#ifndef PHYSICS3D_APP_SCENEPRESENTATION_H
#define PHYSICS3D_APP_SCENEPRESENTATION_H

#include <string>
#include <vector>

#include "input/Camera.h"
#include "render/SceneRenderer.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

namespace app_loop {

struct RuntimeState;
class SimulationController;

void buildHudDebugLines(
    const RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings,
    std::vector<std::string>& lines);

void updatePathHistory(
    const SimulationController& simulation,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings);

void buildTargetHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    OverlayRenderer::TargetHud& targetHud);

void buildPathLines(
    const render_scene::SceneSnapshot& snapshot,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    const ui::InterfaceSettings& interfaceSettings,
    std::vector<OverlayRenderer::ScreenLine>& pathLines);

} // namespace app_loop

#endif // PHYSICS3D_APP_SCENEPRESENTATION_H

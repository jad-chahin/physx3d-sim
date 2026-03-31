#ifndef PHYSICS3D_APP_RENDERSTAGE_H
#define PHYSICS3D_APP_RENDERSTAGE_H

#include <cstdint>

#include "app/AppRuntime.h"
#include "app/SimulationController.h"
#include "app/SpatialHud.h"
#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/FrameRenderer.h"
#include "ui/PauseMenu.h"

struct GLFWwindow;

namespace app_loop {

struct FrameRenderState {
    ui::MenuView menuView{};
    OverlayRenderer::SpatialHud spatialHud{};
    OverlayRenderer::FocusMarker focusMarker{};
    OverlayRenderer::FocusHint focusHint{};
    OverlayRenderer::TargetHud targetHud{};
    SpatialHudScratch spatialHudScratch{};
    render_scene::SceneSnapshot sceneSnapshot{};
    std::uint64_t sceneSnapshotRevisionCounter = 0;
    std::uint64_t cachedStaticSceneRevision = 0;
    bool cachedStaticScene = false;
};

void renderStage(
    GLFWwindow* window,
    const AppLoopState& appState,
    const RuntimeState& runtime,
    const ui::PauseMenu& pauseMenu,
    const input::ControlBindings& controls,
    const SimulationController& simulation,
    const input::Camera& cam,
    render_scene::FrameRenderer& frameRenderer,
    FrameRenderState& frameRenderState);

} // namespace app_loop

#endif // PHYSICS3D_APP_RENDERSTAGE_H

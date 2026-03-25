#ifndef PHYSICS3D_APP_SPATIALHUD_H
#define PHYSICS3D_APP_SPATIALHUD_H

#include "input/Camera.h"
#include "render/SceneRenderer.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

namespace app_loop {

struct SpatialHudScratch {
    struct MinimapCandidate {
        float distanceSq = 0.0f;
        OverlayRenderer::MinimapMarker marker{};
    };

    std::vector<MinimapCandidate> candidates{};
    std::vector<MinimapCandidate> filteredCandidates{};
};

void buildSpatialHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView,
    int targetIndex,
    SpatialHudScratch& scratch,
    OverlayRenderer::SpatialHud& spatialHud);

} // namespace app_loop

#endif // PHYSICS3D_APP_SPATIALHUD_H

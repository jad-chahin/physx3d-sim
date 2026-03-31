#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestRegistry.h"

#include "app/SpatialHud.h"
#include "render/SceneRenderer.h"
#include "ui/MinimapLayout.h"
#include "ui/PauseMenu.h"

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] bool nearlyEqual(const float lhs, const float rhs, const float epsilon = 1e-4f) {
    return std::abs(lhs - rhs) <= epsilon;
}

void testMinimapLayoutKeepsInRangeMarkersOutOfEdgeStyle()
{
    const auto layout = ui::minimap::computeMarkerLayout(
        50.0f,
        18.0f,
        0.0f,
        0.0f,
        100.0f,
        100.0f,
        6.0f,
        1.0f,
        false);

    require(layout.showHeightCue, "height cue should be enabled for tall markers");
    require(!layout.visualEdgeClamped, "in-range markers should not be marked as edge-clamped");
    require(nearlyEqual(layout.markerX, 50.0f), "in-range markers should preserve projected X");
    require(nearlyEqual(layout.markerY, 18.0f), "in-range markers should preserve projected Y");
}

void testMinimapCueClampKeepsTopAndBottomSymbolsVisible()
{
    const float topAdjustedY = ui::minimap::clampMarkerYForCue(
        6.0f,
        0.0f,
        100.0f,
        2.7f,
        2.0f,
        3.6f,
        true);
    const float bottomAdjustedY = ui::minimap::clampMarkerYForCue(
        94.0f,
        0.0f,
        100.0f,
        2.7f,
        2.0f,
        3.6f,
        false);

    require(topAdjustedY > 6.0f, "top markers should shift down enough to keep the upward symbol visible");
    require(bottomAdjustedY < 94.0f, "bottom markers should shift up enough to keep the downward symbol visible");
}

void testSpatialHudRetainsCameraStateWithoutBodies()
{
    ui::InterfaceSettings interfaceSettings{};
    interfaceSettings.showMinimap = true;
    interfaceSettings.showCoordinates = true;
    interfaceSettings.minimapZoomIndex = 2;

    input::Camera cam{};
    cam.pos = glm::vec3(12.0f, 34.0f, -56.0f);

    const render_scene::SceneSnapshot snapshot{};
    render_scene::SceneView sceneView{};
    sceneView.forward = glm::normalize(glm::vec3(0.62f, 0.45f, -0.64f));
    OverlayRenderer::SpatialHud spatialHud{};
    app_loop::SpatialHudScratch scratch{};
    app_loop::buildSpatialHud(snapshot, interfaceSettings, cam, sceneView, -1, scratch, spatialHud);

    const glm::vec2 flatForward(sceneView.forward.x, -sceneView.forward.z);
    const glm::vec2 normalizedForward = glm::normalize(flatForward);
    require(nearlyEqual(spatialHud.worldX, cam.pos.x), "spatial HUD should preserve camera X without bodies");
    require(nearlyEqual(spatialHud.worldY, cam.pos.y), "spatial HUD should preserve camera Y without bodies");
    require(nearlyEqual(spatialHud.worldZ, cam.pos.z), "spatial HUD should preserve camera Z without bodies");
    require(nearlyEqual(
                spatialHud.rangeMeters,
                ui::kMinimapZoomChoices[static_cast<std::size_t>(interfaceSettings.minimapZoomIndex)]),
        "spatial HUD should preserve the configured minimap range without bodies");
    require(nearlyEqual(spatialHud.forwardX, normalizedForward.x) && nearlyEqual(spatialHud.forwardY, normalizedForward.y),
        "spatial HUD should preserve camera heading without bodies");
    require(nearlyEqual(spatialHud.lookPitch, sceneView.forward.y),
        "spatial HUD should preserve camera pitch without bodies");
    require(spatialHud.markers.empty(), "spatial HUD should not emit markers when there are no bodies");
}

void testSpatialHudKeepsVerticallyStackedBodiesSeparate()
{
    ui::InterfaceSettings interfaceSettings{};
    interfaceSettings.showMinimap = true;
    interfaceSettings.showCoordinates = false;

    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 0.0f);

    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = glm::vec3(20.0f, 8.0f, 20.0f),
        .radius = 1.0f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0,
        .materialName = "STEEL",
    });
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = glm::vec3(20.0f, -8.0f, 20.0f),
        .radius = 1.0f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0,
        .materialName = "STEEL",
    });

    render_scene::SceneView sceneView{};
    sceneView.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    OverlayRenderer::SpatialHud spatialHud{};
    app_loop::SpatialHudScratch scratch{};
    app_loop::buildSpatialHud(snapshot, interfaceSettings, cam, sceneView, -1, scratch, spatialHud);

    require(spatialHud.markers.size() == 2, "stacked bodies should survive minimap dedup as separate markers");
    require(spatialHud.markers[0].aboveCamera != spatialHud.markers[1].aboveCamera,
        "stacked bodies should preserve opposite altitude directions");
}

void testSpatialHudKeepsSameCellBodiesWhenUnderMarkerCap()
{
    ui::InterfaceSettings interfaceSettings{};
    interfaceSettings.showMinimap = true;
    interfaceSettings.showCoordinates = false;

    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 0.0f);

    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = glm::vec3(10.0f, 0.0f, 10.0f),
        .radius = 1.0f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0,
        .materialName = "STEEL",
    });
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = glm::vec3(11.0f, 0.0f, 10.0f),
        .radius = 1.0f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0,
        .materialName = "STEEL",
    });

    render_scene::SceneView sceneView{};
    sceneView.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    OverlayRenderer::SpatialHud spatialHud{};
    app_loop::SpatialHudScratch scratch{};
    app_loop::buildSpatialHud(snapshot, interfaceSettings, cam, sceneView, -1, scratch, spatialHud);

    require(spatialHud.markers.size() == 2,
        "same-cell bodies should both remain visible when under the minimap marker cap");
}

} // namespace

void appendMinimapTests(test_registry::TestList& tests)
{
    tests.emplace_back(
        "minimap_layout_keeps_in_range_markers_out_of_edge_style",
        testMinimapLayoutKeepsInRangeMarkersOutOfEdgeStyle);
    tests.emplace_back(
        "minimap_cue_clamp_keeps_top_and_bottom_symbols_visible",
        testMinimapCueClampKeepsTopAndBottomSymbolsVisible);
    tests.emplace_back(
        "spatial_hud_retains_camera_state_without_bodies",
        testSpatialHudRetainsCameraStateWithoutBodies);
    tests.emplace_back(
        "spatial_hud_keeps_vertically_stacked_bodies_separate",
        testSpatialHudKeepsVerticallyStackedBodiesSeparate);
    tests.emplace_back(
        "spatial_hud_keeps_same_cell_bodies_when_under_marker_cap",
        testSpatialHudKeepsSameCellBodiesWhenUnderMarkerCap);
}

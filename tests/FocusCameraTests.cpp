#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "app/AppLoopInternal.h"
#include "app/CameraFocus.h"
#include "app/ScenePresentation.h"

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] render_scene::SceneSnapshot makeSnapshot(const std::vector<render_scene::BodySnapshot>& bodies) {
    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies = bodies;
    return snapshot;
}

void testBeginCameraFocusLocksToTarget()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, focus),
        "begin focus should accept a valid target");
    require(focus.active && focus.bodyId == 42,
        "begin focus should activate and store the target body id");
    require(focus.followDistance >= 7.4f && focus.followDistance <= 19.0f,
        "begin focus should choose a cinematic framing distance");

    const glm::vec3 expectedDirection = glm::normalize(glm::vec3(0.0f, 2.0f * 0.22f, -30.0f));
    require(glm::dot(input::forwardDir(cam), expectedDirection) > 0.999f,
        "begin focus should orient the camera toward the focused body");
}

void testUpdateCameraFocusTracksMovingTarget()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, focus),
        "begin focus should succeed before follow updates");

    snapshot.bodies[0].renderPosition = glm::vec3(10.0f, 0.0f, 0.0f);
    require(app_loop::updateCameraFocus(snapshot, 0.1f, 1.0, cam, focus),
        "follow update should keep tracking a live target");
    require(cam.pos.x > 1.0f,
        "follow update should move the camera laterally with the target");
    require(cam.pos.z < 30.0f,
        "follow update should ease the camera toward the framing distance");
}

void testUpdateCameraFocusClearsMissingTarget()
{
    input::Camera cam{};
    app_loop::CameraFocusState focus{};
    focus.active = true;
    focus.bodyId = 99;
    focus.followDistance = 12.0f;

    const render_scene::SceneSnapshot snapshot{};
    require(!app_loop::updateCameraFocus(snapshot, 0.1f, 1.0, cam, focus),
        "follow update should fail when the focused body no longer exists");
    require(!focus.active && focus.bodyId == 0 && focus.followDistance == 0.0f,
        "missing targets should clear focus state");
}

void testHighSimulationSpeedBoostsFocusCatchUp()
{
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(10.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    input::Camera slowCam{};
    slowCam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    input::Camera fastCam = slowCam;
    app_loop::CameraFocusState slowFocus{
        .active = true,
        .bodyId = 42,
        .followDistance = 18.0f,
    };
    app_loop::CameraFocusState fastFocus = slowFocus;
    input::setLookDirection(slowCam, glm::vec3(0.0f, 2.0f * 0.22f, -30.0f));
    fastCam = slowCam;

    require(app_loop::updateCameraFocus(snapshot, 0.016f, 1.0, slowCam, slowFocus),
        "normal-speed follow update should succeed");
    require(app_loop::updateCameraFocus(snapshot, 0.016f, 64.0, fastCam, fastFocus),
        "high-speed follow update should succeed");
    require(fastCam.pos.x > slowCam.pos.x,
        "high simulation speed should increase focus catch-up strength");
}

void testUpdateCameraFocusKeepsTargetCentered()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 52,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, focus),
        "begin focus should succeed before centered follow updates");

    input::setLookDirection(cam, glm::vec3(8.0f, 0.8f, -30.0f));
    snapshot.bodies[0].renderPosition = glm::vec3(6.0f, 0.0f, 0.0f);

    require(app_loop::updateCameraFocus(snapshot, 0.1f, 32.0, cam, focus),
        "follow update should succeed while re-centering the target");

    const glm::vec3 focusPoint =
        snapshot.bodies[0].renderPosition + glm::vec3(0.0f, snapshot.bodies[0].radius * 0.22f, 0.0f);
    require(glm::dot(input::forwardDir(cam), glm::normalize(focusPoint - cam.pos)) > 0.999f,
        "follow update should keep the focused body centered after smoothing");
}

void testFocusedBodyLookupUsesStableBodyId()
{
    app_loop::CameraFocusState focus{};
    focus.active = true;
    focus.bodyId = 77;
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(-2.0f, 0.0f, 0.0f),
            .radius = 1.0f,
            .bodyId = 12,
            .materialName = "DEFAULT",
        },
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(3.0f, 0.0f, 0.0f),
            .radius = 1.0f,
            .bodyId = 77,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::findFocusedBodyIndex(snapshot, focus) == 1,
        "focused target lookup should follow stable body ids instead of transient indices");
}

void testTargetSelectionIgnoresBodiesPastFarPlane()
{
    input::Camera cam{};
    const render_scene::FramebufferSize framebufferSize{1920, 1080};
    const ui::CameraSettings cameraSettings{};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, cameraSettings, framebufferSize);
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = cam.pos + sceneView.forward * (sceneView.farPlane + 10.0f),
            .radius = 1.0f,
            .bodyId = 101,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::findTargetBodyIndex(snapshot, cam, sceneView) == -1,
        "target selection should ignore bodies clipped by the far plane");
}

void testTargetSelectionKeepsNearClippedBodiesSelectable()
{
    input::Camera cam{};
    const render_scene::FramebufferSize framebufferSize{1920, 1080};
    const ui::CameraSettings cameraSettings{};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, cameraSettings, framebufferSize);
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = cam.pos + sceneView.forward * 1.05f,
            .radius = 1.0f,
            .bodyId = 202,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::findTargetBodyIndex(snapshot, cam, sceneView) == 0,
        "target selection should keep near-clipped bodies under the crosshair selectable");
}

void testTargetSelectionPrefersFrontObjectWhenInsideLargerBody()
{
    input::Camera cam{};
    const render_scene::FramebufferSize framebufferSize{1920, 1080};
    const ui::CameraSettings cameraSettings{};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, cameraSettings, framebufferSize);
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = cam.pos + sceneView.forward * 20.0f,
            .radius = 30.0f,
            .bodyId = 301,
            .materialName = "DEFAULT",
        },
        render_scene::BodySnapshot{
            .renderPosition = cam.pos + sceneView.forward * 10.0f,
            .radius = 1.0f,
            .bodyId = 302,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::findTargetBodyIndex(snapshot, cam, sceneView) == 1,
        "target selection should keep ordering by the first positive hit when inside another body");
}

void testFocusMarkerRejectsBodiesOutsideViewDepth()
{
    input::Camera cam{};
    const render_scene::FramebufferSize framebufferSize{1920, 1080};
    const ui::CameraSettings cameraSettings{};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, cameraSettings, framebufferSize);
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = cam.pos + sceneView.forward * (sceneView.farPlane + 10.0f),
            .radius = 1.0f,
            .bodyId = 401,
            .materialName = "DEFAULT",
        },
    });
    OverlayRenderer::FocusMarker focusMarker{};

    app_loop::buildFocusMarker(snapshot, sceneView, framebufferSize, 0, focusMarker);

    require(!focusMarker.visible,
        "focus marker should stay hidden when the focused body is clipped by view depth");
}

void testMovementExitDoesNotCancelFocusOnActivationFrame()
{
    require(!app_loop::movementCanExitCameraFocus(false, true),
        "movement should not exit focus on the same frame focus starts");
    require(!app_loop::movementCanExitCameraFocus(true, false),
        "pause menu input should not exit focus");
    require(app_loop::movementCanExitCameraFocus(false, false),
        "movement should exit focus once focus is already active during gameplay");
}

} // namespace

void appendFocusCameraTests(std::vector<std::pair<std::string, std::function<void()>>>& tests)
{
    tests.emplace_back("begin_camera_focus_locks_to_target", testBeginCameraFocusLocksToTarget);
    tests.emplace_back("update_camera_focus_tracks_moving_target", testUpdateCameraFocusTracksMovingTarget);
    tests.emplace_back("update_camera_focus_clears_missing_target", testUpdateCameraFocusClearsMissingTarget);
    tests.emplace_back("high_simulation_speed_boosts_focus_catch_up", testHighSimulationSpeedBoostsFocusCatchUp);
    tests.emplace_back("update_camera_focus_keeps_target_centered", testUpdateCameraFocusKeepsTargetCentered);
    tests.emplace_back("focused_body_lookup_uses_stable_body_id", testFocusedBodyLookupUsesStableBodyId);
    tests.emplace_back("target_selection_ignores_bodies_past_far_plane", testTargetSelectionIgnoresBodiesPastFarPlane);
    tests.emplace_back(
        "target_selection_keeps_near_clipped_bodies_selectable",
        testTargetSelectionKeepsNearClippedBodiesSelectable);
    tests.emplace_back(
        "target_selection_prefers_front_object_when_inside_larger_body",
        testTargetSelectionPrefersFrontObjectWhenInsideLargerBody);
    tests.emplace_back(
        "focus_marker_rejects_bodies_outside_view_depth",
        testFocusMarkerRejectsBodiesOutsideViewDepth);
    tests.emplace_back(
        "movement_exit_does_not_cancel_focus_on_activation_frame",
        testMovementExitDoesNotCancelFocusOnActivationFrame);
}

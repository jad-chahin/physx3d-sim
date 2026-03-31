#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestRegistry.h"

#include "app/CameraFocus.h"
#include "app/FrameUpdate.h"
#include "app/ScenePresentation.h"

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] bool nearlyEqual(const float lhs, const float rhs, const float epsilon = 1e-4f) {
    return std::abs(lhs - rhs) <= epsilon;
}

[[nodiscard]] bool nearlyEqualVec3(
    const glm::vec3& lhs,
    const glm::vec3& rhs,
    const float epsilon = 1e-4f)
{
    return nearlyEqual(lhs.x, rhs.x, epsilon) &&
        nearlyEqual(lhs.y, rhs.y, epsilon) &&
        nearlyEqual(lhs.z, rhs.z, epsilon);
}

[[nodiscard]] render_scene::SceneSnapshot makeSnapshot(const std::vector<render_scene::BodySnapshot>& bodies) {
    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies = bodies;
    return snapshot;
}

[[nodiscard]] glm::vec3 focusPointForBody(const render_scene::BodySnapshot& body) {
    return body.renderPosition + glm::vec3(0.0f, body.radius * 0.22f, 0.0f);
}

void testBeginCameraFocusLocksToTarget()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    const ui::CameraSettings cameraSettings{};
    const auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, cameraSettings.fovDegrees, focus),
        "begin focus should accept a valid target");
    require(focus.active && focus.bodyId == 42,
        "begin focus should activate and store the target body id");
    require(focus.followDistance > 10.0f && focus.followDistance < 20.0f,
        "begin focus should choose a closer inspection distance for the target");

    const glm::vec3 focusPoint = focusPointForBody(snapshot.bodies[0]);
    const glm::vec3 expectedDirection = glm::normalize(focusPoint - cam.pos);
    require(glm::dot(input::forwardDir(cam), expectedDirection) > 0.999f,
        "begin focus should orient the camera toward the focused body");
    require(nearlyEqual(glm::length(cam.pos - focusPoint), focus.followDistance, 1e-3f),
        "begin focus should place the camera on the chosen orbit radius");
}

void testUpdateCameraFocusTracksMovingTarget()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    const ui::CameraSettings cameraSettings{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, cameraSettings.fovDegrees, focus),
        "begin focus should succeed before follow updates");
    const glm::vec3 originalPosition = cam.pos;

    snapshot.bodies[0].renderPosition = glm::vec3(10.0f, 0.0f, 0.0f);
    require(app_loop::updateCameraFocus(snapshot, 0.0f, cameraSettings.fovDegrees, cam, focus),
        "follow update should keep tracking a live target");
    require(nearlyEqualVec3(cam.pos - originalPosition, glm::vec3(10.0f, 0.0f, 0.0f), 1e-3f),
        "follow update should carry the camera with the focused target instead of lagging behind");
}

void testUpdateCameraFocusClearsMissingTarget()
{
    input::Camera cam{};
    app_loop::CameraFocusState focus{};
    focus.active = true;
    focus.bodyId = 99;
    focus.followDistance = 12.0f;

    const render_scene::SceneSnapshot snapshot{};
    require(!app_loop::updateCameraFocus(snapshot, 0.0f, 60.0f, cam, focus),
        "follow update should fail when the focused body no longer exists");
    require(!focus.active && focus.bodyId == 0 && focus.followDistance == 0.0f,
        "missing targets should clear focus state");
}

void testUpdateCameraFocusAppliesScrollZoom()
{
    const ui::CameraSettings cameraSettings{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 42,
            .materialName = "DEFAULT",
        },
    });

    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};

    require(app_loop::beginCameraFocus(snapshot, 0, cam, cameraSettings.fovDegrees, focus),
        "begin focus should succeed before zoom updates");

    const float originalDistance = focus.followDistance;
    require(app_loop::updateCameraFocus(snapshot, 1.0f, cameraSettings.fovDegrees, cam, focus),
        "positive scroll should zoom the focus camera");
    require(focus.followDistance < originalDistance,
        "scrolling up should zoom the focus camera inward");

    const glm::vec3 focusPoint = focusPointForBody(snapshot.bodies[0]);
    require(nearlyEqual(glm::length(cam.pos - focusPoint), focus.followDistance, 1e-3f),
        "zooming should keep the camera on the focused orbit radius");

    const float zoomedInDistance = focus.followDistance;
    require(app_loop::updateCameraFocus(snapshot, -1.0f, cameraSettings.fovDegrees, cam, focus),
        "negative scroll should zoom the focus camera back out");
    require(focus.followDistance > zoomedInDistance,
        "scrolling down should zoom the focus camera outward");

    require(app_loop::updateCameraFocus(snapshot, 100.0f, cameraSettings.fovDegrees, cam, focus),
        "large positive scroll should still keep focus valid");
    require(focus.followDistance > snapshot.bodies[0].radius + 0.5f,
        "zooming in aggressively should stay outside the focused body");
}

void testUpdateCameraFocusKeepsTargetCentered()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    const ui::CameraSettings cameraSettings{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 52,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, cameraSettings.fovDegrees, focus),
        "begin focus should succeed before centered follow updates");

    input::setLookDirection(cam, glm::vec3(8.0f, 0.8f, -30.0f));
    snapshot.bodies[0].renderPosition = glm::vec3(6.0f, 0.0f, 0.0f);

    require(app_loop::updateCameraFocus(snapshot, 0.0f, cameraSettings.fovDegrees, cam, focus),
        "follow update should succeed while re-centering the target");

    const glm::vec3 focusPoint = focusPointForBody(snapshot.bodies[0]);
    require(glm::dot(input::forwardDir(cam), glm::normalize(focusPoint - cam.pos)) > 0.999f,
        "follow update should keep the focused body centered while orbiting");
    require(nearlyEqual(glm::length(cam.pos - focusPoint), focus.followDistance, 1e-3f),
        "follow update should preserve the requested orbit distance");
}

void testUpdateCameraFocusPreservesOrbitSideForMovingTarget()
{
    input::Camera cam{};
    cam.pos = glm::vec3(0.0f, 0.0f, 30.0f);
    app_loop::CameraFocusState focus{};
    const ui::CameraSettings cameraSettings{};
    auto snapshot = makeSnapshot({
        render_scene::BodySnapshot{
            .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = 2.0f,
            .bodyId = 62,
            .materialName = "DEFAULT",
        },
    });

    require(app_loop::beginCameraFocus(snapshot, 0, cam, cameraSettings.fovDegrees, focus),
        "begin focus should succeed before orbit-preservation updates");

    const glm::vec3 initialFocusPoint = focusPointForBody(snapshot.bodies[0]);
    const glm::vec3 initialOffsetDirection = glm::normalize(cam.pos - initialFocusPoint);

    snapshot.bodies[0].renderPosition = glm::vec3(20.0f, 0.0f, 0.0f);

    require(app_loop::updateCameraFocus(snapshot, 0.0f, cameraSettings.fovDegrees, cam, focus),
        "follow update should succeed while carrying the camera with an orbiting target");

    const glm::vec3 movedFocusPoint = focusPointForBody(snapshot.bodies[0]);
    require(glm::dot(glm::normalize(cam.pos - movedFocusPoint), initialOffsetDirection) > 0.995f,
        "follow update should preserve the camera's orbit side as the target moves");
}

void testSetLookDirectionKeepsYawForVerticalDirections()
{
    input::Camera cam{};
    cam.yaw = 1.2345f;

    input::setLookDirection(cam, glm::vec3(0.0f, 5.0f, 0.0f));

    require(nearlyEqual(cam.yaw, 1.2345f, 1e-5f),
        "vertical look directions should preserve yaw instead of snapping to an arbitrary heading");
    require(cam.pitch > glm::radians(88.0f),
        "vertical look directions should still point the camera nearly straight up");
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

void appendFocusCameraTests(test_registry::TestList& tests)
{
    tests.emplace_back("begin_camera_focus_locks_to_target", testBeginCameraFocusLocksToTarget);
    tests.emplace_back("update_camera_focus_tracks_moving_target", testUpdateCameraFocusTracksMovingTarget);
    tests.emplace_back("update_camera_focus_clears_missing_target", testUpdateCameraFocusClearsMissingTarget);
    tests.emplace_back("update_camera_focus_applies_scroll_zoom", testUpdateCameraFocusAppliesScrollZoom);
    tests.emplace_back("update_camera_focus_keeps_target_centered", testUpdateCameraFocusKeepsTargetCentered);
    tests.emplace_back(
        "update_camera_focus_preserves_orbit_side_for_moving_target",
        testUpdateCameraFocusPreservesOrbitSideForMovingTarget);
    tests.emplace_back(
        "set_look_direction_keeps_yaw_for_vertical_directions",
        testSetLookDirectionKeepsYawForVerticalDirections);
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

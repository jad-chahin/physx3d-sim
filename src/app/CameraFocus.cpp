#include "app/CameraFocus.h"

#include <algorithm>
#include <cmath>

namespace app_loop {
namespace {

constexpr float kFocusHeightBiasMultiplier = 0.22f;
constexpr float kFocusDistancePadding = 1.04f;
constexpr float kFocusMinDistanceMultiplier = 1.35f;
constexpr float kFocusMinDistancePadding = 0.75f;
constexpr float kFocusDefaultScreenRadiusFraction = 0.20f;
constexpr float kFocusMaxZoomMultiplier = 4.0f;
constexpr float kFocusScrollZoomStep = 0.2f;

struct FocusDistanceRange {
    float minDistance = 0.0f;
    float framingDistance = 0.0f;
    float maxDistance = 0.0f;
};

[[nodiscard]] glm::vec3 focusPointForBody(const render_scene::BodySnapshot& body) {
    return body.renderPosition + glm::vec3(0.0f, body.radius * kFocusHeightBiasMultiplier, 0.0f);
}

[[nodiscard]] FocusDistanceRange focusDistanceRange(
    const render_scene::BodySnapshot& body,
    const float fovDegrees)
{
    const float minDistance = std::max(body.radius * kFocusMinDistanceMultiplier, body.radius + kFocusMinDistancePadding);
    const float clampedFovDegrees = std::clamp(fovDegrees, 30.0f, 130.0f);
    const float halfFovRadians = glm::radians(clampedFovDegrees * 0.5f);
    const float projectionScale = 1.0f / std::tan(std::max(halfFovRadians, 0.01f));
    const float framingDistance = std::max(
        minDistance,
        body.radius * projectionScale / kFocusDefaultScreenRadiusFraction);
    const float maxDistance = std::max(framingDistance * kFocusMaxZoomMultiplier, minDistance + 1.0f);
    return FocusDistanceRange{minDistance, framingDistance, maxDistance};
}

[[nodiscard]] float desiredFocusDistance(
    const render_scene::BodySnapshot& body,
    const input::Camera& cam,
    const float fovDegrees)
{
    const float currentDistance = glm::length(focusPointForBody(body) - cam.pos) * kFocusDistancePadding;
    const FocusDistanceRange range = focusDistanceRange(body, fovDegrees);
    return std::clamp(currentDistance, range.minDistance, range.framingDistance);
}

void applyFocusZoomDelta(
    const float scrollDeltaY,
    const FocusDistanceRange& range,
    CameraFocusState& focusState)
{
    if (std::abs(scrollDeltaY) <= 0.01f) {
        return;
    }

    const float zoomScale = std::exp(-scrollDeltaY * kFocusScrollZoomStep);
    focusState.followDistance = std::clamp(focusState.followDistance * zoomScale, range.minDistance, range.maxDistance);
}

} // namespace

int findFocusedBodyIndex(const render_scene::SceneSnapshot& snapshot, const CameraFocusState& focusState)
{
    if (!focusState.active) {
        return -1;
    }

    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        if (snapshot.bodies[i].bodyId == focusState.bodyId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void clearCameraFocus(CameraFocusState& focusState) {
    focusState = {};
}

bool beginCameraFocus(
    const render_scene::SceneSnapshot& snapshot,
    const int targetBodyIndex,
    input::Camera& cam,
    const float fovDegrees,
    CameraFocusState& focusState)
{
    if (targetBodyIndex < 0 || targetBodyIndex >= static_cast<int>(snapshot.bodies.size())) {
        return false;
    }

    const auto& body = snapshot.bodies[static_cast<std::size_t>(targetBodyIndex)];
    const glm::vec3 focusPoint = focusPointForBody(body);
    input::setLookDirection(cam, focusPoint - cam.pos);
    focusState.active = true;
    focusState.bodyId = body.bodyId;
    focusState.followDistance = desiredFocusDistance(body, cam, fovDegrees);
    cam.pos = focusPoint - input::forwardDir(cam) * focusState.followDistance;
    return true;
}

bool updateCameraFocus(
    const render_scene::SceneSnapshot& snapshot,
    const float scrollDeltaY,
    const float fovDegrees,
    input::Camera& cam,
    CameraFocusState& focusState)
{
    const int focusedIndex = findFocusedBodyIndex(snapshot, focusState);
    if (focusedIndex < 0) {
        clearCameraFocus(focusState);
        return false;
    }

    const auto& body = snapshot.bodies[static_cast<std::size_t>(focusedIndex)];
    const glm::vec3 focusPoint = focusPointForBody(body);
    const FocusDistanceRange range = focusDistanceRange(body, fovDegrees);
    if (focusState.followDistance <= 0.0f) {
        focusState.followDistance = desiredFocusDistance(body, cam, fovDegrees);
    } else {
        focusState.followDistance = std::clamp(focusState.followDistance, range.minDistance, range.maxDistance);
    }
    applyFocusZoomDelta(scrollDeltaY, range, focusState);

    // Focus mode is an exact orbit camera: the target stays centered and mouse look
    // changes the orbit angles directly instead of being damped by follow smoothing.
    cam.pos = focusPoint - input::forwardDir(cam) * focusState.followDistance;
    return true;
}

} // namespace app_loop

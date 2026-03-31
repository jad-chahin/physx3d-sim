#include "app/CameraFocus.h"

#include <algorithm>
#include <cmath>

namespace app_loop {
namespace {

constexpr float kFocusMinDistanceMultiplier = 3.7f;
constexpr float kFocusMaxDistanceMultiplier = 9.5f;
constexpr float kFocusDistancePadding = 1.06f;
constexpr float kFocusHeightBiasMultiplier = 0.22f;
constexpr float kFocusSmoothing = 6.2f;
constexpr float kFocusErrorCatchUpScale = 2.5f;

[[nodiscard]] glm::vec3 focusPointForBody(const render_scene::BodySnapshot& body) {
    return body.renderPosition + glm::vec3(0.0f, body.radius * kFocusHeightBiasMultiplier, 0.0f);
}

[[nodiscard]] float desiredFocusDistance(const render_scene::BodySnapshot& body, const input::Camera& cam) {
    const float currentDistance = glm::length(focusPointForBody(body) - cam.pos) * kFocusDistancePadding;
    const float minDistance = std::max(body.radius * kFocusMinDistanceMultiplier, body.radius + 0.75f);
    const float maxDistance = std::max(minDistance, body.radius * kFocusMaxDistanceMultiplier);
    return std::clamp(currentDistance, minDistance, maxDistance);
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
    focusState.followDistance = desiredFocusDistance(body, cam);
    return true;
}

bool updateCameraFocus(
    const render_scene::SceneSnapshot& snapshot,
    const float frameTime,
    const double simulationSpeed,
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
    const float minDistance = std::max(body.radius * kFocusMinDistanceMultiplier, body.radius + 0.75f);
    focusState.followDistance = std::max(focusState.followDistance, minDistance);
    const glm::vec3 desiredPosition = focusPoint - input::forwardDir(cam) * focusState.followDistance;
    const float error = glm::length(desiredPosition - cam.pos);
    const float normalizedError = error / std::max(focusState.followDistance, 1.0f);
    const float catchUpBoost = 1.0f + std::clamp(normalizedError * kFocusErrorCatchUpScale, 0.0f, 6.0f);
    const float speedBoost = std::clamp(
        std::sqrt(static_cast<float>(std::max(1.0, simulationSpeed))),
        1.0f,
        8.0f);
    const float blend =
        1.0f - std::exp(-(kFocusSmoothing * catchUpBoost * speedBoost) * std::max(frameTime, 0.0f));
    cam.pos += (desiredPosition - cam.pos) * blend;
    input::setLookDirection(cam, focusPoint - cam.pos);
    return true;
}

} // namespace app_loop

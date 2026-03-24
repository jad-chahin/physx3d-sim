#include "app/SpatialHud.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace app_loop {
namespace {

constexpr float kMinimapClampLimit = 0.92f;
constexpr std::size_t kMaxMinimapMarkers = 96;
constexpr int kMinimapGridResolution = 14;
constexpr int kMinimapHeightBandsPerSide = 5;
constexpr int kMinimapHeightBandCount = kMinimapHeightBandsPerSide * 2;

bool raySphereHit(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& center,
    const float radius,
    float& outT)
{
    const glm::vec3 oc = rayOrigin - center;
    const float b = glm::dot(oc, rayDir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }

    const float s = std::sqrt(disc);
    const float t0 = -b - s;
    const float t1 = -b + s;
    if (t0 > 0.0f) {
        outT = t0;
        return true;
    }
    if (t1 > 0.0f) {
        outT = t1;
        return true;
    }
    return false;
}

int findTargetBodyIndex(
    const render_scene::SceneSnapshot& snapshot,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView)
{
    const glm::vec3 rayOrigin = cam.pos;
    const glm::vec3 rayDir = glm::normalize(sceneView.forward);

    int bestIdx = -1;
    float bestT = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        float t = 0.0f;
        if (!raySphereHit(
                rayOrigin,
                rayDir,
                snapshot.bodies[i].renderPosition,
                snapshot.bodies[i].radius,
                t))
        {
            continue;
        }
        if (t < bestT) {
            bestT = t;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

struct MinimapCandidate {
    float distanceSq = 0.0f;
    OverlayRenderer::MinimapMarker marker{};
};

[[nodiscard]] int minimapCellIndex(const float xNorm, const float yNorm) {
    const float tx = std::clamp((xNorm + 1.0f) * 0.5f, 0.0f, 0.9999f);
    const float ty = std::clamp((yNorm + 1.0f) * 0.5f, 0.0f, 0.9999f);
    const int cellX = static_cast<int>(tx * static_cast<float>(kMinimapGridResolution));
    const int cellY = static_cast<int>(ty * static_cast<float>(kMinimapGridResolution));
    return cellY * kMinimapGridResolution + cellX;
}

[[nodiscard]] int minimapHeightBand(const OverlayRenderer::MinimapMarker& marker) {
    const float normalizedHeight = std::clamp(std::sqrt(marker.heightNorm), 0.0f, 0.9999f);
    const int magnitudeBand =
        static_cast<int>(normalizedHeight * static_cast<float>(kMinimapHeightBandsPerSide));
    return marker.aboveCamera ? kMinimapHeightBandsPerSide + magnitudeBand : magnitudeBand;
}

[[nodiscard]] int minimapBucketIndex(const OverlayRenderer::MinimapMarker& marker) {
    return minimapCellIndex(marker.xNorm, marker.yNorm) * kMinimapHeightBandCount + minimapHeightBand(marker);
}

[[nodiscard]] bool shouldReplaceMinimapMarker(
    const MinimapCandidate& candidate,
    const MinimapCandidate& existing)
{
    if (candidate.marker.highlighted != existing.marker.highlighted) {
        return candidate.marker.highlighted;
    }
    if (candidate.marker.edgeClamped != existing.marker.edgeClamped) {
        return !candidate.marker.edgeClamped;
    }
    if (candidate.marker.dynamicBody != existing.marker.dynamicBody) {
        return candidate.marker.dynamicBody;
    }
    return candidate.distanceSq < existing.distanceSq;
}

} // namespace

void buildSpatialHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView,
    OverlayRenderer::SpatialHud& spatialHud)
{
    spatialHud.worldX = cam.pos.x;
    spatialHud.worldY = cam.pos.y;
    spatialHud.worldZ = cam.pos.z;
    const int zoomIndex =
        std::clamp(interfaceSettings.minimapZoomIndex, 0, static_cast<int>(ui::kMinimapZoomChoices.size()) - 1);
    spatialHud.rangeMeters = ui::kMinimapZoomChoices[static_cast<std::size_t>(zoomIndex)];
    spatialHud.forwardX = 0.0f;
    spatialHud.forwardY = -1.0f;
    spatialHud.lookPitch = std::clamp(sceneView.forward.y, -1.0f, 1.0f);
    spatialHud.markers.clear();

    if (!interfaceSettings.showHud || (!interfaceSettings.showCoordinates && !interfaceSettings.showMinimap)) {
        return;
    }

    const glm::vec2 flatForward(sceneView.forward.x, -sceneView.forward.z);
    const float flatLenSq = glm::dot(flatForward, flatForward);
    if (flatLenSq > 1e-6f) {
        const glm::vec2 normalizedForward = flatForward / std::sqrt(flatLenSq);
        spatialHud.forwardX = normalizedForward.x;
        spatialHud.forwardY = normalizedForward.y;
    }

    if (!interfaceSettings.showMinimap || snapshot.bodies.empty()) {
        return;
    }

    const int targetIndex = findTargetBodyIndex(snapshot, cam, sceneView);
    std::vector<MinimapCandidate> candidates;
    candidates.reserve(snapshot.bodies.size());

    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        const glm::vec3 delta = snapshot.bodies[i].renderPosition - cam.pos;
        const float xNorm = delta.x / spatialHud.rangeMeters;
        const float yNorm = -delta.z / spatialHud.rangeMeters;
        const float clampedX = std::clamp(xNorm, -kMinimapClampLimit, kMinimapClampLimit);
        const float clampedY = std::clamp(yNorm, -kMinimapClampLimit, kMinimapClampLimit);
        candidates.push_back({
            delta.x * delta.x + delta.z * delta.z,
            {
                clampedX,
                clampedY,
                std::clamp(std::abs(delta.y) / spatialHud.rangeMeters, 0.0f, 1.0f),
                static_cast<int>(i) == targetIndex,
                snapshot.bodies[i].invMass > 0.0,
                delta.y > 0.0f,
                clampedX != xNorm || clampedY != yNorm,
            },
        });
    }

    std::vector<MinimapCandidate> filteredCandidates;
    filteredCandidates.reserve(candidates.size());
    if (candidates.size() <= kMaxMinimapMarkers) {
        filteredCandidates = candidates;
    } else {
        std::array<int, kMinimapGridResolution * kMinimapGridResolution * kMinimapHeightBandCount> bestIndices;
        bestIndices.fill(-1);
        for (const auto& candidate : candidates) {
            if (candidate.marker.highlighted) {
                filteredCandidates.push_back(candidate);
                continue;
            }

            const int bucketIndex = minimapBucketIndex(candidate.marker);
            const int existingIndex = bestIndices[static_cast<std::size_t>(bucketIndex)];
            if (existingIndex < 0) {
                bestIndices[static_cast<std::size_t>(bucketIndex)] = static_cast<int>(filteredCandidates.size());
                filteredCandidates.push_back(candidate);
                continue;
            }

            if (shouldReplaceMinimapMarker(candidate, filteredCandidates[static_cast<std::size_t>(existingIndex)])) {
                filteredCandidates[static_cast<std::size_t>(existingIndex)] = candidate;
            }
        }
    }

    std::stable_sort(
        filteredCandidates.begin(),
        filteredCandidates.end(),
        [](const MinimapCandidate& a, const MinimapCandidate& b) {
            if (a.marker.highlighted != b.marker.highlighted) {
                return a.marker.highlighted > b.marker.highlighted;
            }
            if (a.marker.dynamicBody != b.marker.dynamicBody) {
                return a.marker.dynamicBody > b.marker.dynamicBody;
            }
            if (a.marker.edgeClamped != b.marker.edgeClamped) {
                return a.marker.edgeClamped < b.marker.edgeClamped;
            }
            return a.distanceSq < b.distanceSq;
        });

    const std::size_t markerCount = std::min(kMaxMinimapMarkers, filteredCandidates.size());
    spatialHud.markers.reserve(markerCount);
    for (std::size_t i = 0; i < markerCount; ++i) {
        spatialHud.markers.push_back(filteredCandidates[i].marker);
    }
}

} // namespace app_loop

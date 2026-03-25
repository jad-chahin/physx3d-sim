#include "app/SpatialHud.h"
#include "app/ScenePresentation.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace app_loop {
namespace {

constexpr float kMinimapClampLimit = 0.92f;
constexpr std::size_t kMaxMinimapMarkers = 96;
constexpr int kMinimapGridResolution = 14;
constexpr int kMinimapHeightBandsPerSide = 5;
constexpr int kMinimapHeightBandCount = kMinimapHeightBandsPerSide * 2;

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
    const SpatialHudScratch::MinimapCandidate& candidate,
    const SpatialHudScratch::MinimapCandidate& existing)
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
    const int targetIndex,
    SpatialHudScratch& scratch,
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

    if (!interfaceSettings.showCoordinates && !interfaceSettings.showMinimap) {
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

    scratch.candidates.clear();
    scratch.candidates.reserve(snapshot.bodies.size());

    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        const glm::vec3 delta = snapshot.bodies[i].renderPosition - cam.pos;
        const float xNorm = delta.x / spatialHud.rangeMeters;
        const float yNorm = -delta.z / spatialHud.rangeMeters;
        const float clampedX = std::clamp(xNorm, -kMinimapClampLimit, kMinimapClampLimit);
        const float clampedY = std::clamp(yNorm, -kMinimapClampLimit, kMinimapClampLimit);
        scratch.candidates.push_back({
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

    scratch.filteredCandidates.clear();
    scratch.filteredCandidates.reserve(scratch.candidates.size());
    if (scratch.candidates.size() <= kMaxMinimapMarkers) {
        scratch.filteredCandidates = scratch.candidates;
    } else {
        std::array<int, kMinimapGridResolution * kMinimapGridResolution * kMinimapHeightBandCount> bestIndices{};
        bestIndices.fill(-1);
        for (const auto& candidate : scratch.candidates) {
            if (candidate.marker.highlighted) {
                scratch.filteredCandidates.push_back(candidate);
                continue;
            }

            const int bucketIndex = minimapBucketIndex(candidate.marker);
            const int existingIndex = bestIndices[static_cast<std::size_t>(bucketIndex)];
            if (existingIndex < 0) {
                bestIndices[static_cast<std::size_t>(bucketIndex)] = static_cast<int>(scratch.filteredCandidates.size());
                scratch.filteredCandidates.push_back(candidate);
                continue;
            }

            if (shouldReplaceMinimapMarker(candidate, scratch.filteredCandidates[static_cast<std::size_t>(existingIndex)])) {
                scratch.filteredCandidates[static_cast<std::size_t>(existingIndex)] = candidate;
            }
        }
    }

    const auto markerOrder = [](const SpatialHudScratch::MinimapCandidate& a, const SpatialHudScratch::MinimapCandidate& b) {
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
        };

    const std::size_t markerCount = std::min(kMaxMinimapMarkers, scratch.filteredCandidates.size());
    if (scratch.filteredCandidates.size() > markerCount) {
        std::ranges::partial_sort(scratch.filteredCandidates, scratch.filteredCandidates.begin() + static_cast<std::ptrdiff_t>(markerCount)
                                  ,
                                  markerOrder);
    } else {
        std::ranges::stable_sort(scratch.filteredCandidates
                                 ,
                                 markerOrder);
    }
    spatialHud.markers.reserve(markerCount);
    for (std::size_t i = 0; i < markerCount; ++i) {
        spatialHud.markers.push_back(scratch.filteredCandidates[i].marker);
    }
}

} // namespace app_loop

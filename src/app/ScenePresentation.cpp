#include "app/ScenePresentation.h"

#include "app/AppLoopInternal.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace app_loop {
namespace {

constexpr double kPathSampleSpacing = 0.2;
constexpr float kFocusMarkerMinRadiusPx = 14.0f;
constexpr float kFocusMarkerMaxRadiusPx = 84.0f;

struct ScreenProjection {
    bool visible = false;
    float xPx = 0.0f;
    float yPx = 0.0f;
    float clipW = 0.0f;
};

[[nodiscard]] bool ndcInClipVolume(const glm::vec3& ndc)
{
    return ndc.x >= -1.0f && ndc.x <= 1.0f &&
        ndc.y >= -1.0f && ndc.y <= 1.0f &&
        ndc.z >= -1.0f && ndc.z <= 1.0f;
}

bool resizePathTrail(render_scene::PathTrail& trail, const std::size_t maxPathPoints) {
    if (maxPathPoints == 0 || trail.count == 0 || trail.points.empty()) {
        if (maxPathPoints == 0) {
            const bool hadData = !trail.points.empty() || trail.count != 0 || trail.start != 0;
            trail = {};
            return hadData;
        }
        return false;
    }

    const bool needsTrim = trail.points.size() > maxPathPoints || trail.count > maxPathPoints;
    const bool needsLinearizeForGrowth = trail.start != 0 && trail.points.size() < maxPathPoints;
    if (!needsTrim && !needsLinearizeForGrowth) {
        return false;
    }

    const std::size_t keepCount = std::min({trail.count, trail.points.size(), maxPathPoints});
    std::vector<sim::Vec3> recentPoints;
    recentPoints.reserve(keepCount);
    const std::size_t keepStart = (trail.start + trail.count - keepCount) % trail.points.size();
    for (std::size_t i = 0; i < keepCount; ++i) {
        recentPoints.push_back(trail.points[(keepStart + i) % trail.points.size()]);
    }

    trail.points = std::move(recentPoints);
    trail.start = 0;
    trail.count = trail.points.size();
    return true;
}

bool pushPathPoint(render_scene::PathTrail& trail, const sim::Vec3& position, const std::size_t maxPathPoints) {
    if (maxPathPoints == 0) {
        const bool hadData = !trail.points.empty() || trail.count != 0 || trail.start != 0;
        trail = {};
        return hadData;
    }

    resizePathTrail(trail, maxPathPoints);

    if (trail.points.size() < maxPathPoints) {
        trail.points.push_back(position);
        trail.count = trail.points.size();
        return true;
    }

    const std::size_t writeIndex = (trail.start + trail.count) % trail.points.size();
    trail.points[writeIndex] = position;
    if (trail.count < trail.points.size()) {
        ++trail.count;
    } else {
        trail.start = (trail.start + 1) % trail.points.size();
    }
    return true;
}

bool appendPathSamples(
    render_scene::PathTrail& trail,
    const sim::Vec3& position,
    const double sampleSpacing,
    const std::size_t maxPathPoints)
{
    if (trail.count == 0) {
        return pushPathPoint(trail, position, maxPathPoints);
    }

    const std::size_t lastIndex = (trail.start + trail.count - 1) % trail.points.size();
    const sim::Vec3 origin = trail.points[lastIndex];
    const sim::Vec3 delta = position - origin;
    const double distanceSq = delta.dot(delta);
    if (distanceSq <= 1e-12) {
        return false;
    }

    const double distance = std::sqrt(distanceSq);
    if (distance < sampleSpacing) {
        return false;
    }

    const sim::Vec3 direction = delta * (1.0 / distance);
    const auto sampleCount = static_cast<std::size_t>(distance / sampleSpacing);
    bool changed = false;
    for (std::size_t sample = 1; sample <= sampleCount; ++sample) {
        changed = pushPathPoint(
            trail,
            origin + direction * (sampleSpacing * static_cast<double>(sample)),
            maxPathPoints) || changed;
    }
    return changed;
}

bool raySphereDepthRange(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& center,
    const float radius,
    float& outNearT,
    float& outFarT)
{
    const glm::vec3 offset = rayOrigin - center;
    const float b = glm::dot(offset, rayDir);
    const float c = glm::dot(offset, offset) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }

    const float root = std::sqrt(disc);
    outNearT = -b - root;
    outFarT = -b + root;
    return outFarT > 0.0f;
}

int findTargetBodyIndexImpl(
    const render_scene::SceneSnapshot& snapshot,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView)
{
    const glm::vec3 rayOrigin = cam.pos;
    const glm::vec3 rayDir = glm::normalize(sceneView.forward);

    int bestIdx = -1;
    float bestT = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        float nearT = 0.0f;
        float farT = 0.0f;
        if (!raySphereDepthRange(
                rayOrigin,
                rayDir,
                snapshot.bodies[i].renderPosition,
                snapshot.bodies[i].radius,
                nearT,
                farT))
        {
            continue;
        }
        if (farT < sceneView.nearPlane || nearT > sceneView.farPlane) {
            continue;
        }
        const float firstPositiveHitT = nearT > 0.0f ? nearT : farT;
        if (firstPositiveHitT < bestT) {
            bestT = firstPositiveHitT;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

[[nodiscard]] ScreenProjection projectPointToScreen(
    const glm::vec3& worldPoint,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize)
{
    if (framebufferSize.w <= 0 || framebufferSize.h <= 0) {
        return {};
    }

    const glm::vec4 clip = sceneView.viewProj * glm::vec4(worldPoint, 1.0f);
    if (clip.w <= 0.0f) {
        return {};
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (!ndcInClipVolume(ndc)) {
        return {};
    }

    return ScreenProjection{
        true,
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(framebufferSize.w),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(framebufferSize.h),
        clip.w,
    };
}

[[nodiscard]] float projectedRadiusPx(
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    const float radius,
    const float clipW)
{
    const float projectedRadius =
        (sceneView.proj[1][1] * radius / clipW) * static_cast<float>(framebufferSize.h) * 0.5f;
    return std::clamp(projectedRadius, kFocusMarkerMinRadiusPx, kFocusMarkerMaxRadiusPx);
}

} // namespace

int findTargetBodyIndex(
    const render_scene::SceneSnapshot& snapshot,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView)
{
    return findTargetBodyIndexImpl(snapshot, cam, sceneView);
}

void updatePathHistory(
    const SimulationController& simulation,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings)
{
    if (!interfaceSettings.drawPath) {
        if (!runtime.pathHistory.empty()) {
            runtime.pathHistory.clear();
            ++runtime.pathHistoryRevision;
        }
        return;
    }

    const auto& bodies = simulation.bodies();
    bool historyChanged = false;
    if (runtime.pathHistory.size() != bodies.size()) {
        runtime.pathHistory.resize(bodies.size());
        historyChanged = true;
    }
    const int pathLengthIndex =
        std::clamp(interfaceSettings.pathLengthIndex, 0, static_cast<int>(ui::kPathLengthChoices.size()) - 1);
    const auto maxPathPoints =
        static_cast<std::size_t>(ui::kPathLengthChoices[static_cast<std::size_t>(pathLengthIndex)]);

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        auto& history = runtime.pathHistory[i];
        historyChanged = resizePathTrail(history, maxPathPoints) || historyChanged;
        const double spacing = std::max(kPathSampleSpacing, bodies[i].radius * 0.2);
        historyChanged = appendPathSamples(history, bodies[i].position, spacing, maxPathPoints) || historyChanged;
    }

    if (historyChanged) {
        ++runtime.pathHistoryRevision;
    }
}

void buildTargetHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    const int targetBodyIndex,
    OverlayRenderer::TargetHud& targetHud)
{
    targetHud.visible = false;
    targetHud.xPx = 0.0f;
    targetHud.yPx = 0.0f;
    targetHud.lines.clear();
    if (!interfaceSettings.objectInfo || snapshot.bodies.empty()) {
        return;
    }

    if (targetBodyIndex < 0) {
        return;
    }

    const auto i = static_cast<std::size_t>(targetBodyIndex);
    const glm::vec3 labelPos =
        snapshot.bodies[i].renderPosition + glm::vec3(0.0f, snapshot.bodies[i].radius * 1.6f, 0.0f);
    const ScreenProjection labelProjection = projectPointToScreen(labelPos, sceneView, framebufferSize);
    if (!labelProjection.visible) {
        return;
    }

    targetHud.visible = true;
    targetHud.xPx = labelProjection.xPx;
    targetHud.yPx = labelProjection.yPx;
    targetHud.lines.reserve(6);

    char line[64];
    if (interfaceSettings.objectInfoMaterial) {
        const auto materialName = snapshot.bodies[i].materialName;
        const char* materialData = materialName.empty() ? "" : materialName.data();
        std::snprintf(
            line,
            sizeof(line),
            "MAT:%.*s",
            static_cast<int>(materialName.size()),
            materialData);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoVelocity) {
        std::snprintf(line, sizeof(line), "V:%.2f", snapshot.bodies[i].velocityMagnitude);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoAngularSpeed) {
        std::snprintf(line, sizeof(line), "A:%.2f", snapshot.bodies[i].angularSpeedMagnitude);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoRadius) {
        std::snprintf(line, sizeof(line), "R:%.2f", snapshot.bodies[i].radius);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoMass) {
        if (snapshot.bodies[i].invMass > 0.0) {
            std::snprintf(line, sizeof(line), "M:%.2f", 1.0 / snapshot.bodies[i].invMass);
        } else {
            std::snprintf(line, sizeof(line), "M:INF");
        }
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoBodyType) {
        std::snprintf(line, sizeof(line), "T:%s", snapshot.bodies[i].invMass > 0.0 ? "DYNAMIC" : "STATIC");
        targetHud.lines.emplace_back(line);
    }
}

void buildFocusMarker(
    const render_scene::SceneSnapshot& snapshot,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    const int focusedBodyIndex,
    OverlayRenderer::FocusMarker& focusMarker)
{
    focusMarker = {};
    if (focusedBodyIndex < 0 || focusedBodyIndex >= static_cast<int>(snapshot.bodies.size()) ||
        framebufferSize.w <= 0 || framebufferSize.h <= 0)
    {
        return;
    }

    const auto i = static_cast<std::size_t>(focusedBodyIndex);
    const ScreenProjection bodyProjection =
        projectPointToScreen(snapshot.bodies[i].renderPosition, sceneView, framebufferSize);
    if (!bodyProjection.visible) {
        return;
    }

    focusMarker.visible = true;
    focusMarker.xPx = bodyProjection.xPx;
    focusMarker.yPx = bodyProjection.yPx;
    focusMarker.radiusPx = projectedRadiusPx(
        sceneView,
        framebufferSize,
        snapshot.bodies[i].radius,
        bodyProjection.clipW);
}

} // namespace app_loop

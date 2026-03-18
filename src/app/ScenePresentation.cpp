#include "app/ScenePresentation.h"

#include "app/AppLoopInternal.h"

#include <cstdio>
#include <limits>

namespace app_loop {
namespace {

constexpr std::size_t kMaxPathPoints = 128;

void pushPathPoint(render_scene::PathTrail& trail, const sim::Vec3& position) {
    if (trail.points.size() < kMaxPathPoints) {
        trail.points.push_back(position);
        trail.count = trail.points.size();
        return;
    }

    const std::size_t writeIndex = (trail.start + trail.count) % trail.points.size();
    trail.points[writeIndex] = position;
    if (trail.count < trail.points.size()) {
        ++trail.count;
    } else {
        trail.start = (trail.start + 1) % trail.points.size();
    }
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
        if (!render_scene::raySphereHit(
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

} // namespace

void buildHudDebugLines(
    const RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings,
    std::vector<std::string>& lines)
{
    lines.clear();
    if (!interfaceSettings.showPhysicsStats) {
        return;
    }

    const sim::World::Metrics& metrics = runtime.hudMetrics.displayed;
    lines.reserve(5);

    char line[96];
    std::snprintf(line, sizeof(line), "GRAV B:%zu P:%zu", metrics.gravityBodies, metrics.gravityPairs);
    lines.emplace_back(line);

    std::snprintf(
        line,
        sizeof(line),
        "BP D:%zu S:%zu  VIS:%zu",
        metrics.broadphaseCandidatesDiscrete,
        metrics.broadphaseCandidatesSwept,
        metrics.pairsVisited);
    lines.emplace_back(line);

    std::snprintf(
        line,
        sizeof(line),
        "IMP:%zu  WARM:%zu  MAN:%zu",
        metrics.pairsImpulseApplied,
        metrics.warmStartedPairs,
        metrics.manifoldActivePairs);
    lines.emplace_back(line);

    std::snprintf(line, sizeof(line), "CCD:%zu  ZERO:%zu", metrics.ccdEvents, metrics.ccdZeroTimePairsResolved);
    lines.emplace_back(line);

    std::snprintf(line, sizeof(line), "SAN B:%zu  F:%zu", metrics.sanitizedBodies, metrics.sanitizedFields);
    lines.emplace_back(line);
}

void updatePathHistory(
    const SimulationController& simulation,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings)
{
    if (!interfaceSettings.drawPath) {
        runtime.pathHistory.clear();
        return;
    }

    const auto& bodies = simulation.bodies();
    runtime.pathHistory.resize(bodies.size());

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        auto& history = runtime.pathHistory[i];
        const sim::Vec3& position = bodies[i].position;
        if (history.count == 0) {
            pushPathPoint(history, position);
            continue;
        }
        const std::size_t lastIndex = (history.start + history.count - 1) % history.points.size();
        const sim::Vec3 delta = position - history.points[lastIndex];
        if (delta.dot(delta) > 1e-8) {
            pushPathPoint(history, position);
        }
    }
}

void buildTargetHud(
    const render_scene::SceneSnapshot& snapshot,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    OverlayRenderer::TargetHud& targetHud)
{
    targetHud.visible = false;
    targetHud.xPx = 0.0f;
    targetHud.yPx = 0.0f;
    targetHud.lines.clear();
    if (!interfaceSettings.objectInfo || snapshot.bodies.empty()) {
        return;
    }

    const int bestIdx = findTargetBodyIndex(snapshot, cam, sceneView);
    if (bestIdx < 0) {
        return;
    }

    const auto i = static_cast<std::size_t>(bestIdx);
    const glm::vec3 labelPos =
        snapshot.bodies[i].renderPosition + glm::vec3(0.0f, snapshot.bodies[i].radius * 1.6f, 0.0f);
    const glm::vec4 clip = sceneView.viewProj * glm::vec4(labelPos, 1.0f);
    if (clip.w <= 0.0f) {
        return;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
        return;
    }

    targetHud.visible = true;
    targetHud.xPx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(framebufferSize.w);
    targetHud.yPx = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(framebufferSize.h);
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

void buildPathLines(
    const render_scene::SceneSnapshot& snapshot,
    const render_scene::SceneView& sceneView,
    const render_scene::FramebufferSize& framebufferSize,
    const ui::InterfaceSettings& interfaceSettings,
    std::vector<OverlayRenderer::ScreenLine>& pathLines)
{
    pathLines.clear();
    if (!interfaceSettings.drawPath) {
        return;
    }

    pathLines.reserve(snapshot.pathTrails.size() * (kMaxPathPoints - 1));
    for (const auto& history : snapshot.pathTrails) {
        if (history.count < 2 || history.points.empty()) {
            continue;
        }
        for (std::size_t segment = 1; segment < history.count; ++segment) {
            const std::size_t idx0 = (history.start + segment - 1) % history.points.size();
            const std::size_t idx1 = (history.start + segment) % history.points.size();
            const sim::Vec3& p0 = history.points[idx0];
            const sim::Vec3& p1 = history.points[idx1];

            const glm::vec4 clip0 = sceneView.viewProj *
                glm::vec4(static_cast<float>(p0.x), static_cast<float>(p0.y), static_cast<float>(p0.z), 1.0f);
            const glm::vec4 clip1 = sceneView.viewProj *
                glm::vec4(static_cast<float>(p1.x), static_cast<float>(p1.y), static_cast<float>(p1.z), 1.0f);
            if (clip0.w <= 0.0f || clip1.w <= 0.0f) {
                continue;
            }

            const glm::vec3 ndc0 = glm::vec3(clip0) / clip0.w;
            const glm::vec3 ndc1 = glm::vec3(clip1) / clip1.w;
            if (ndc0.x < -1.0f || ndc0.x > 1.0f || ndc0.y < -1.0f || ndc0.y > 1.0f ||
                ndc1.x < -1.0f || ndc1.x > 1.0f || ndc1.y < -1.0f || ndc1.y > 1.0f)
            {
                continue;
            }

            pathLines.push_back({
                (ndc0.x * 0.5f + 0.5f) * static_cast<float>(framebufferSize.w),
                (1.0f - (ndc0.y * 0.5f + 0.5f)) * static_cast<float>(framebufferSize.h),
                (ndc1.x * 0.5f + 0.5f) * static_cast<float>(framebufferSize.w),
                (1.0f - (ndc1.y * 0.5f + 0.5f)) * static_cast<float>(framebufferSize.h),
            });
        }
    }
}

} // namespace app_loop

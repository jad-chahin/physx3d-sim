#include "app/AppLoopInternal.h"

#include <glm/glm.hpp>

namespace app_loop {
namespace {
    [[nodiscard]] glm::mat3 quatToMat3(const sim::Quaternion& q) {
        const float w = static_cast<float>(q.w);
        const float x = static_cast<float>(q.x);
        const float y = static_cast<float>(q.y);
        const float z = static_cast<float>(q.z);

        const float xx = x * x;
        const float yy = y * y;
        const float zz = z * z;
        const float xy = x * y;
        const float xz = x * z;
        const float yz = y * z;
        const float wx = w * x;
        const float wy = w * y;
        const float wz = w * z;

        return glm::mat3(
            1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy),
            2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),
            2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy));
    }

} // namespace

void buildSceneSnapshot(
    const SimulationController& simulation,
    const RuntimeState& runtime,
    const double alpha,
    render_scene::SceneSnapshot& snapshot)
{
    const auto& bodies = simulation.bodies();
    snapshot.bodies.resize(bodies.size());
    snapshot.models.resize(bodies.size());
    snapshot.pathTrails = runtime.pathHistory;

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const sim::Vec3 interpolatedPosition =
            bodies[i].prevPosition + (bodies[i].position - bodies[i].prevPosition) * alpha;
        const sim::Quaternion interpolatedOrientation =
            sim::nlerpQuat(bodies[i].prevOrientation, bodies[i].orientation, alpha);
        const glm::vec3 pos(
            static_cast<float>(interpolatedPosition.x),
            static_cast<float>(interpolatedPosition.y),
            static_cast<float>(interpolatedPosition.z));
        const float radius = static_cast<float>(bodies[i].radius);

        snapshot.bodies[i] = render_scene::BodySnapshot{
            .renderPosition = pos,
            .radius = radius,
            .velocityMagnitude = static_cast<float>(bodies[i].velocity.magnitude()),
            .angularSpeedMagnitude = static_cast<float>(bodies[i].angularVelocity.magnitude()),
            .invMass = bodies[i].invMass,
            .materialName = bodies[i].materialName,
        };

        glm::mat4 model(1.0f);
        const glm::mat3 rotation = quatToMat3(interpolatedOrientation);
        model[0] = glm::vec4(rotation[0] * radius, 0.0f);
        model[1] = glm::vec4(rotation[1] * radius, 0.0f);
        model[2] = glm::vec4(rotation[2] * radius, 0.0f);
        model[3][0] = pos.x;
        model[3][1] = pos.y;
        model[3][2] = pos.z;
        snapshot.models[i] = model;
    }
}

void drawOverlay(
    const OverlayRenderer& overlay,
    const render_scene::FramebufferSize& framebufferSize,
    const RuntimeState& runtime,
    const ui::MenuView& menuView,
    const OverlayRenderer::TargetHud& targetHud,
    const std::vector<OverlayRenderer::ScreenLine>& pathLines,
    const float uiScale,
    const ui::InterfaceSettings& interfaceSettings,
    const std::vector<std::string>& hudDebugLines)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    overlay.draw(
        framebufferSize.w,
        framebufferSize.h,
        runtime.simulation.simFrozen,
        runtime.simulation.simSpeed,
        runtime.fps.displayed,
        menuView,
        targetHud,
        pathLines,
        uiScale,
        interfaceSettings.showHud,
        interfaceSettings.showCrosshair,
        hudDebugLines);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace app_loop


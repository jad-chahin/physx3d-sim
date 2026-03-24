#ifndef PHYSICS3D_RENDER_SCENERENDERER_H
#define PHYSICS3D_RENDER_SCENERENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <string_view>
#include <span>
#include <vector>

#include "input/Camera.h"
#include "sim/Vec3.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

namespace render_scene {

struct FramebufferSize {
    int w = 0;
    int h = 0;
};

struct SceneView {
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewProj{1.0f};
};

struct InstanceBufferState {
    std::size_t capacity = 0;
};

struct PathBufferState {
    std::size_t capacity = 0;
};

struct PathTrail {
    std::vector<sim::Vec3> points{};
    std::size_t start = 0;
    std::size_t count = 0;
};

struct BodySnapshot {
    glm::vec3 renderPosition{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    float velocityMagnitude = 0.0f;
    float angularSpeedMagnitude = 0.0f;
    double invMass = 0.0;
    std::string_view materialName{};
};

struct SceneSnapshot {
    std::vector<BodySnapshot> bodies;
    std::vector<glm::mat4> models;
    std::span<const PathTrail> pathTrails{};
};

bool raySphereHit(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& center,
    float radius,
    float& outT);

bool worldToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    int fbw,
    int fbh,
    float& outXPx,
    float& outYPx);

SceneView buildSceneView(
    const input::Camera& cam,
    const ui::CameraSettings& cameraSettings,
    const FramebufferSize& framebufferSize);

void drawBodies(
    GLuint instanceVbo,
    GLuint program,
    GLint uView,
    GLint uProj,
    GLint uLightDir,
    GLint uBaseColor,
    GLint uAmbient,
    GLuint sphereVao,
    GLsizei sphereIndexCount,
    const SceneView& sceneView,
    InstanceBufferState& instanceBufferState,
    const SceneSnapshot& snapshot);

void drawPathTrails(
    GLuint pathVao,
    GLuint pathVbo,
    GLuint program,
    GLint uViewProj,
    GLint uColor,
    const SceneView& sceneView,
    PathBufferState& pathBufferState,
    std::vector<glm::vec3>& scratchPoints,
    std::vector<GLint>& drawStarts,
    std::vector<GLsizei>& drawCounts,
    std::span<const PathTrail> pathTrails,
    bool simFrozen);

} // namespace render_scene

#endif // PHYSICS3D_RENDER_SCENERENDERER_H

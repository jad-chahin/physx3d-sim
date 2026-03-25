#ifndef PHYSICS3D_RENDER_SCENELIGHTING_H
#define PHYSICS3D_RENDER_SCENELIGHTING_H

#include "render/SceneRenderer.h"

#include <array>
namespace render_scene {

enum class BackdropPreset {
    Sunny,
    Space,
    Nebula,
};

struct LocalLight {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float range = 0.0f;
    glm::vec3 color{0.0f, 0.0f, 0.0f};
    float intensity = 0.0f;
};

struct SceneLighting {
    BackdropPreset backdropPreset = BackdropPreset::Sunny;
    glm::vec3 direction{0.35203f, 0.82476f, 0.44256f};
    glm::vec3 color{4.8f, 4.45f, 4.1f};
    glm::vec3 skyZenithColor{0.18f, 0.36f, 0.66f};
    glm::vec3 skyHorizonColor{0.74f, 0.80f, 0.88f};
    glm::vec3 groundColor{0.06f, 0.05f, 0.04f};
    glm::vec3 sunGlowColor{1.30f, 1.08f, 0.86f};
    glm::vec3 skyAccentColor{0.48f, 0.62f, 0.82f};
    float starIntensity = 0.0f;
    float nebulaIntensity = 0.0f;
    float bloomStrength = 0.18f;
    glm::vec3 fogNearColor{0.55f, 0.63f, 0.74f};
    float fogNearDistance = 32.0f;
    glm::vec3 fogFarColor{0.78f, 0.84f, 0.92f};
    float fogFarDistance = 150.0f;
    std::array<LocalLight, kMaxLocalLights> localLights{};
    int localLightCount = 0;
    glm::mat4 shadowViewProj{1.0f};
    glm::vec2 shadowTexelSize{0.0f, 0.0f};
};

[[nodiscard]] SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    std::span<const SceneInstanceData> instances,
    int shadowMapResolution);

[[nodiscard]] SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    std::span<const SceneInstanceData> instances,
    int shadowMapResolution,
    BackdropPreset backdropPreset);

[[nodiscard]] SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    int shadowMapResolution);

[[nodiscard]] SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    int shadowMapResolution,
    BackdropPreset backdropPreset);

} // namespace render_scene

#endif // PHYSICS3D_RENDER_SCENELIGHTING_H

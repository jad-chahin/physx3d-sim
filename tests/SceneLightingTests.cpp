#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestRegistry.h"

#include "render/SceneLighting.h"

namespace {

void require(const bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] bool nearlyEqual(const float lhs, const float rhs, const float epsilon = 1e-4f)
{
    return std::abs(lhs - rhs) <= epsilon;
}

void testBuildSceneLightingProducesFiniteStateWithoutBodies()
{
    render_scene::SceneView sceneView{};
    sceneView.cameraPosition = glm::vec3(3.0f, 7.0f, -9.0f);
    sceneView.forward = glm::normalize(glm::vec3(0.24f, -0.11f, -0.96f));

    const render_scene::SceneSnapshot snapshot{};
    const render_scene::SceneLighting lighting = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        2048,
        render_scene::BackdropPreset::Sunny);

    require(nearlyEqual(glm::length(lighting.direction), 1.0f, 1e-3f),
        "directional light should stay normalized");
    require(nearlyEqual(lighting.shadowTexelSize.x, 1.0f / 2048.0f),
        "shadow texel size should reflect the configured shadow-map resolution");
    require(nearlyEqual(lighting.shadowTexelSize.y, 1.0f / 2048.0f),
        "shadow texel size should be square");
    require(glm::length(lighting.skyZenithColor) > glm::length(lighting.groundColor),
        "zenith environment lighting should remain brighter than ground lighting");
    require(glm::length(lighting.skyHorizonColor) > glm::length(lighting.groundColor),
        "horizon environment lighting should remain brighter than ground lighting");
    require(glm::length(lighting.sunGlowColor) > glm::length(lighting.skyHorizonColor),
        "sun glow should stay brighter than the horizon fill");
    require(lighting.fogNearDistance > 0.0f && lighting.fogFarDistance > lighting.fogNearDistance,
        "fog distances should stay positive and ordered");
    require(glm::length(lighting.fogFarColor) >= glm::length(lighting.fogNearColor),
        "far fog should stay at least as bright as near fog for depth separation");
    require(lighting.localLightCount == 0,
        "empty scenes should not synthesize local lights");

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            require(std::isfinite(lighting.shadowViewProj[column][row]),
                "shadow view-projection should remain finite without scene bodies");
        }
    }
}

void testBuildSceneLightingKeepsNearbyBodyInsideShadowFrustum()
{
    render_scene::SceneView sceneView{};
    sceneView.cameraPosition = glm::vec3(0.0f, 2.0f, 0.0f);
    sceneView.forward = glm::normalize(glm::vec3(0.18f, -0.12f, -0.98f));

    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = sceneView.cameraPosition + sceneView.forward * 20.0f + glm::vec3(0.0f, 3.0f, 0.0f),
        .radius = 2.0f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0,
        .materialName = "STEEL",
    });

    const render_scene::SceneLighting lighting = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        1024,
        render_scene::BackdropPreset::Sunny);
    const glm::vec4 clip = lighting.shadowViewProj * glm::vec4(snapshot.bodies.front().renderPosition, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    require(std::abs(ndc.x) <= 1.0f, "nearby tracked body should stay inside shadow frustum width");
    require(std::abs(ndc.y) <= 1.0f, "nearby tracked body should stay inside shadow frustum height");
    require(ndc.z >= -1.0f && ndc.z <= 1.0f,
        "nearby tracked body should stay inside shadow frustum depth");
}

void testBuildSceneLightingExtractsLocalLightFromEmissiveBody()
{
    render_scene::SceneView sceneView{};
    sceneView.cameraPosition = glm::vec3(0.0f, 0.0f, 24.0f);
    sceneView.forward = glm::vec3(0.0f, 0.0f, -1.0f);

    render_scene::SceneSnapshot snapshot{};
    snapshot.bodies.push_back(render_scene::BodySnapshot{
        .renderPosition = glm::vec3(0.0f, 0.0f, 0.0f),
        .radius = 4.6f,
        .velocityMagnitude = 0.0f,
        .angularSpeedMagnitude = 0.0f,
        .invMass = 1.0 / ((4.0 / 3.0) * 3.14159265358979323846 * 4.6 * 4.6 * 4.6 * 1.25e9),
        .bodyId = 1,
        .materialName = "DEFAULT",
    });

    const render_scene::SceneLighting lighting = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        1024,
        render_scene::BackdropPreset::Sunny);
    require(lighting.localLightCount == 1, "emissive stellar bodies should produce a local light");
    require(lighting.localLights[0].range > 32.0f, "stellar local lights should reach nearby orbiting bodies");
    require(glm::length(lighting.localLights[0].color) > 0.0f, "local light color should be nonzero");
}

void testBuildSceneLightingCapsLocalLightCount()
{
    render_scene::SceneView sceneView{};
    sceneView.cameraPosition = glm::vec3(0.0f, 0.0f, 8.0f);
    sceneView.forward = glm::vec3(0.0f, 0.0f, -1.0f);

    render_scene::SceneSnapshot snapshot{};
    for (int i = 0; i < 6; ++i) {
        snapshot.bodies.push_back(render_scene::BodySnapshot{
            .renderPosition = glm::vec3(static_cast<float>(i * 7), 0.0f, 0.0f),
            .radius = 4.2f,
            .velocityMagnitude = 0.0f,
            .angularSpeedMagnitude = 0.0f,
            .invMass = 1.0 / ((4.0 / 3.0) * 3.14159265358979323846 * 4.2 * 4.2 * 4.2 * 1.25e9),
            .bodyId = static_cast<std::uint64_t>(i + 1),
            .materialName = "DEFAULT",
        });
    }

    const render_scene::SceneLighting lighting = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        1024,
        render_scene::BackdropPreset::Sunny);
    require(lighting.localLightCount == static_cast<int>(render_scene::kMaxLocalLights),
        "local light extraction should stay capped");
}

void testBuildSceneLightingSupportsBackdropPresets()
{
    render_scene::SceneView sceneView{};
    sceneView.cameraPosition = glm::vec3(-4.0f, 3.0f, 14.0f);
    sceneView.forward = glm::normalize(glm::vec3(0.16f, -0.08f, -0.98f));

    const render_scene::SceneSnapshot snapshot{};
    const render_scene::SceneLighting sunny = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        2048,
        render_scene::BackdropPreset::Sunny);
    const render_scene::SceneLighting space = render_scene::buildSceneLighting(
        sceneView,
        snapshot,
        2048,
        render_scene::BackdropPreset::Space);

    require(sunny.backdropPreset == render_scene::BackdropPreset::Sunny,
        "sunny preset should be recorded in the lighting state");
    require(space.backdropPreset == render_scene::BackdropPreset::Space,
        "space preset should be recorded in the lighting state");
    require(sunny.starIntensity == 0.0f,
        "sunny preset should not enable space-only sky effects");
    require(space.starIntensity > 0.8f,
        "space preset should enable stars");
    require(nearlyEqual(sunny.fogNearDistance, space.fogNearDistance) &&
            nearlyEqual(sunny.fogFarDistance, space.fogFarDistance),
        "backdrop presets should share the same visibility range");
    require(glm::distance(sunny.skyZenithColor, space.skyZenithColor) > 0.2f,
        "space should not collapse to the sunny sky colors");
    require(glm::length(space.groundColor) < glm::length(sunny.groundColor),
        "space backdrop should keep a darker lower hemisphere than sunny");
    require(glm::length(space.skyZenithColor) < 0.02f,
        "space backdrop should keep the sky base genuinely dark");
}

} // namespace

void appendSceneLightingTests(test_registry::TestList& tests)
{
    tests.emplace_back(
        "scene_lighting_produces_finite_state_without_bodies",
        testBuildSceneLightingProducesFiniteStateWithoutBodies);
    tests.emplace_back(
        "scene_lighting_keeps_nearby_body_inside_shadow_frustum",
        testBuildSceneLightingKeepsNearbyBodyInsideShadowFrustum);
    tests.emplace_back(
        "scene_lighting_extracts_local_light_from_emissive_body",
        testBuildSceneLightingExtractsLocalLightFromEmissiveBody);
    tests.emplace_back(
        "scene_lighting_caps_local_light_count",
        testBuildSceneLightingCapsLocalLightCount);
    tests.emplace_back(
        "scene_lighting_supports_backdrop_presets",
        testBuildSceneLightingSupportsBackdropPresets);
}

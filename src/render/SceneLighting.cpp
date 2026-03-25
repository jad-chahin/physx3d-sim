#include "render/SceneLighting.h"
#include "render/RenderMaterial.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace render_scene {
namespace {

// Manual backdrop switch until this is surfaced in settings/UI.
constexpr BackdropPreset kActiveBackdropPreset = BackdropPreset::Space;
constexpr glm::vec3 kDirectionalLightDirection(0.35203f, 0.82476f, 0.44256f);
constexpr float kShadowFocusDistance = 18.0f;
constexpr float kShadowTrackDistance = 55.0f;
constexpr float kFallbackHalfExtent = 22.0f;
constexpr float kBoundsPadding = 6.0f;
constexpr float kLightPullbackDistance = 80.0f;
constexpr float kNearPlanePadding = 12.0f;
constexpr float kFarPlanePadding = 18.0f;
constexpr float kMinOrthoHalfExtent = 16.0f;
constexpr float kMinLocalLightRange = 8.0f;

void expandBounds(glm::vec3& minBounds, glm::vec3& maxBounds, const glm::vec3& point)
{
    minBounds = glm::min(minBounds, point);
    maxBounds = glm::max(maxBounds, point);
}

glm::vec3 selectLightUp(const glm::vec3& lightForward)
{
    if (std::abs(glm::dot(lightForward, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.95f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {0.0f, 1.0f, 0.0f};
}

std::array<glm::vec3, 8> makeBoundsCorners(const glm::vec3& minBounds, const glm::vec3& maxBounds)
{
    return {{
        {minBounds.x, minBounds.y, minBounds.z},
        {minBounds.x, minBounds.y, maxBounds.z},
        {minBounds.x, maxBounds.y, minBounds.z},
        {minBounds.x, maxBounds.y, maxBounds.z},
        {maxBounds.x, minBounds.y, minBounds.z},
        {maxBounds.x, minBounds.y, maxBounds.z},
        {maxBounds.x, maxBounds.y, minBounds.z},
        {maxBounds.x, maxBounds.y, maxBounds.z},
    }};
}

float luminance(const glm::vec3& color)
{
    return glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

void applySunnyBackdrop(SceneLighting& lighting)
{
    const float sunHeight = std::clamp(lighting.direction.y * 0.5f + 0.5f, 0.0f, 1.0f);
    const float daylight = glm::smoothstep(0.15f, 0.85f, sunHeight);
    lighting.color = glm::mix(
        glm::vec3(3.45f, 2.95f, 2.55f),
        glm::vec3(4.8f, 4.45f, 4.1f),
        daylight);
    lighting.skyZenithColor = glm::mix(
        glm::vec3(0.06f, 0.10f, 0.18f),
        glm::vec3(0.18f, 0.36f, 0.66f),
        daylight);
    lighting.skyHorizonColor = glm::mix(
        glm::vec3(0.34f, 0.24f, 0.20f),
        glm::vec3(0.74f, 0.80f, 0.88f),
        daylight);
    lighting.groundColor = glm::mix(
        glm::vec3(0.05f, 0.04f, 0.05f),
        glm::vec3(0.09f, 0.08f, 0.07f),
        daylight);
    lighting.sunGlowColor = glm::mix(
        glm::vec3(1.00f, 0.55f, 0.24f),
        glm::vec3(1.30f, 1.08f, 0.86f),
        daylight);
    lighting.skyAccentColor = glm::mix(
        glm::vec3(0.32f, 0.44f, 0.66f),
        glm::vec3(0.48f, 0.62f, 0.82f),
        daylight);
    lighting.starIntensity = 0.0f;
    lighting.nebulaIntensity = 0.0f;
    lighting.bloomStrength = 0.18f;
    lighting.fogNearColor = glm::mix(lighting.skyHorizonColor, lighting.groundColor, 0.25f);
    lighting.fogFarColor = glm::max(
        glm::mix(lighting.skyHorizonColor, lighting.skyZenithColor, 0.58f),
        lighting.fogNearColor + glm::vec3(0.02f));
    lighting.fogNearDistance = glm::mix(24.0f, 36.0f, daylight);
    lighting.fogFarDistance = glm::mix(90.0f, 165.0f, daylight);
}

void applySpaceBackdrop(SceneLighting& lighting)
{
    lighting.color = glm::vec3(4.20f, 4.45f, 4.95f);
    lighting.skyZenithColor = glm::vec3(0.0018f, 0.0028f, 0.0065f);
    lighting.skyHorizonColor = glm::vec3(0.006f, 0.010f, 0.022f);
    lighting.groundColor = glm::vec3(0.0008f, 0.0012f, 0.0035f);
    lighting.sunGlowColor = glm::vec3(1.04f, 1.12f, 1.28f);
    lighting.skyAccentColor = glm::vec3(0.38f, 0.46f, 0.64f);
    lighting.starIntensity = 1.0f;
    lighting.nebulaIntensity = 0.0f;
    lighting.bloomStrength = 0.0f;
    lighting.fogNearColor = glm::vec3(0.0025f, 0.0040f, 0.0090f);
    lighting.fogFarColor = glm::vec3(0.0055f, 0.0095f, 0.0180f);
    lighting.fogNearDistance = 260.0f;
    lighting.fogFarDistance = 620.0f;
}

void applyNebulaBackdrop(SceneLighting& lighting)
{
    lighting.color = glm::vec3(3.96f, 3.72f, 4.02f);
    lighting.skyZenithColor = glm::vec3(0.0024f, 0.0048f, 0.016f);
    lighting.skyHorizonColor = glm::vec3(0.22f, 0.07f, 0.14f);
    lighting.groundColor = glm::vec3(0.0010f, 0.0024f, 0.008f);
    lighting.sunGlowColor = glm::vec3(1.10f, 0.77f, 0.58f);
    lighting.skyAccentColor = glm::vec3(0.08f, 0.36f, 0.44f);
    lighting.starIntensity = 0.78f;
    lighting.nebulaIntensity = 0.88f;
    lighting.bloomStrength = 0.12f;
    lighting.fogNearColor = glm::vec3(0.011f, 0.008f, 0.020f);
    lighting.fogFarColor = glm::vec3(0.054f, 0.034f, 0.072f);
    lighting.fogNearDistance = 128.0f;
    lighting.fogFarDistance = 320.0f;
}

void applyBackdropPreset(SceneLighting& lighting, const BackdropPreset backdropPreset)
{
    lighting.backdropPreset = backdropPreset;
    switch (backdropPreset) {
        case BackdropPreset::Sunny:
            applySunnyBackdrop(lighting);
            break;
        case BackdropPreset::Space:
            applySpaceBackdrop(lighting);
            break;
        case BackdropPreset::Nebula:
            applyNebulaBackdrop(lighting);
            break;
    }
}

float localLightRange(
    const float radius,
    const glm::vec3& emissiveColor,
    const float emissiveIntensity)
{
    const float emissiveStrength = luminance(emissiveColor) * emissiveIntensity;
    const float scaledRange = radius * glm::mix(18.0f, 34.0f, glm::clamp(emissiveStrength * 0.35f, 0.0f, 1.0f));
    return std::max(kMinLocalLightRange, scaledRange);
}

glm::vec3 localLightColor(const glm::vec3& emissiveColor, const float emissiveIntensity)
{
    const float emissiveStrength = std::max(0.0f, emissiveIntensity);
    return emissiveColor * (0.85f + emissiveStrength * 1.45f);
}

void tryInsertLocalLight(
    SceneLighting& lighting,
    const LocalLight& light)
{
    if (lighting.localLightCount < static_cast<int>(kMaxLocalLights)) {
        lighting.localLights[static_cast<std::size_t>(lighting.localLightCount)] = light;
        ++lighting.localLightCount;
    } else {
        int weakestIndex = 0;
        float weakestScore = std::numeric_limits<float>::infinity();
        for (int i = 0; i < lighting.localLightCount; ++i) {
            const LocalLight& candidate = lighting.localLights[static_cast<std::size_t>(i)];
            const float candidateScore = candidate.intensity;
            if (candidateScore < weakestScore) {
                weakestScore = candidateScore;
                weakestIndex = i;
            }
        }
        if (light.intensity <= weakestScore) {
            return;
        }
        lighting.localLights[static_cast<std::size_t>(weakestIndex)] = light;
    }
}

void sortLocalLights(SceneLighting& lighting)
{
    std::sort(
        lighting.localLights.begin(),
        lighting.localLights.begin() + lighting.localLightCount,
        [](const LocalLight& lhs, const LocalLight& rhs) {
            return lhs.intensity > rhs.intensity;
        });
}

} // namespace

SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    const std::span<const SceneInstanceData> instances,
    const int shadowMapResolution)
{
    return buildSceneLighting(sceneView, snapshot, instances, shadowMapResolution, kActiveBackdropPreset);
}

SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    const int shadowMapResolution)
{
    return buildSceneLighting(sceneView, snapshot, {}, shadowMapResolution, kActiveBackdropPreset);
}

SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    const std::span<const SceneInstanceData> instances,
    const int shadowMapResolution,
    const BackdropPreset backdropPreset)
{
    SceneLighting lighting{};
    lighting.direction = kDirectionalLightDirection;
    applyBackdropPreset(lighting, backdropPreset);

    const glm::vec3 focusCenter = sceneView.cameraPosition + sceneView.forward * kShadowFocusDistance;
    glm::vec3 allBoundsMin = focusCenter - glm::vec3(kFallbackHalfExtent);
    glm::vec3 allBoundsMax = focusCenter + glm::vec3(kFallbackHalfExtent);
    expandBounds(allBoundsMin, allBoundsMax, sceneView.cameraPosition);

    glm::vec3 trackedBoundsMin = allBoundsMin;
    glm::vec3 trackedBoundsMax = allBoundsMax;
    bool trackedBodyFound = false;
    constexpr float maxTrackedDistanceSq = kShadowTrackDistance * kShadowTrackDistance;

    for (std::size_t i = 0; i < snapshot.bodies.size(); ++i) {
        const BodySnapshot& body = snapshot.bodies[i];
        glm::vec3 emissiveColor;
        float emissiveIntensity;
        const glm::vec3 extent = glm::vec3(std::max(body.radius, 0.25f));
        expandBounds(allBoundsMin, allBoundsMax, body.renderPosition - extent);
        expandBounds(allBoundsMin, allBoundsMax, body.renderPosition + extent);

        const glm::vec3 offset = body.renderPosition - focusCenter;
        if (glm::dot(offset, offset) <= maxTrackedDistanceSq) {
            expandBounds(trackedBoundsMin, trackedBoundsMax, body.renderPosition - extent);
            expandBounds(trackedBoundsMin, trackedBoundsMax, body.renderPosition + extent);
            trackedBodyFound = true;
        }

        if (i < instances.size()) {
            const glm::vec4 emissive = instances[i].emissiveColorStrength;
            emissiveColor = glm::vec3(emissive);
            emissiveIntensity = emissive.w;
        } else {
            const RenderMaterial material = resolvePresentationMaterial(RenderMaterialInputs{
                body.materialName,
                body.radius,
                body.invMass,
                body.bodyId,
            });
            emissiveColor = material.emissiveColor;
            emissiveIntensity = material.emissiveIntensity;
        }

        if (!(emissiveIntensity > 0.01f) || glm::length(emissiveColor) <= 1e-4f) {
            continue;
        }

        const float range = localLightRange(body.radius, emissiveColor, emissiveIntensity);
        const glm::vec3 color = localLightColor(emissiveColor, emissiveIntensity);
        const float distanceToCamera = glm::length(body.renderPosition - sceneView.cameraPosition);
        const float distanceWeight = 1.0f / (1.0f + distanceToCamera * 0.03f);
        const float score = luminance(color) * range * distanceWeight;
        tryInsertLocalLight(
            lighting,
            LocalLight{
                body.renderPosition,
                range,
                color,
                score,
            });
    }
    sortLocalLights(lighting);
    if (lighting.localLightCount > 0) {
        lighting.bloomStrength = std::max(lighting.bloomStrength, 0.14f);
    }

    if (shadowMapResolution > 0) {
        const float texelSize = 1.0f / static_cast<float>(shadowMapResolution);
        lighting.shadowTexelSize = glm::vec2(texelSize, texelSize);
    }

    glm::vec3 minBounds = trackedBodyFound ? trackedBoundsMin : allBoundsMin;
    glm::vec3 maxBounds = trackedBodyFound ? trackedBoundsMax : allBoundsMax;

    minBounds -= glm::vec3(kBoundsPadding);
    maxBounds += glm::vec3(kBoundsPadding);

    const glm::vec3 boundsCenter = 0.5f * (minBounds + maxBounds);
    const glm::vec3 lightForward = -lighting.direction;
    const glm::vec3 lightEye = boundsCenter + lighting.direction * kLightPullbackDistance;
    const glm::mat4 lightView = glm::lookAt(lightEye, boundsCenter, selectLightUp(lightForward));

    glm::vec3 lightSpaceMin(std::numeric_limits<float>::infinity());
    glm::vec3 lightSpaceMax(-std::numeric_limits<float>::infinity());
    for (const glm::vec3& corner : makeBoundsCorners(minBounds, maxBounds)) {
        const glm::vec3 lightSpacePoint = glm::vec3(lightView * glm::vec4(corner, 1.0f));
        lightSpaceMin = glm::min(lightSpaceMin, lightSpacePoint);
        lightSpaceMax = glm::max(lightSpaceMax, lightSpacePoint);
    }

    const float orthoHalfWidth = std::max(
        kMinOrthoHalfExtent,
        std::max(std::abs(lightSpaceMin.x), std::abs(lightSpaceMax.x)));
    const float orthoHalfHeight = std::max(
        kMinOrthoHalfExtent,
        std::max(std::abs(lightSpaceMin.y), std::abs(lightSpaceMax.y)));
    const float nearPlane = std::max(0.1f, -lightSpaceMax.z - kNearPlanePadding);
    const float farPlane = std::max(nearPlane + 1.0f, -lightSpaceMin.z + kFarPlanePadding);

    const glm::mat4 lightProj = glm::ortho(
        -orthoHalfWidth,
        orthoHalfWidth,
        -orthoHalfHeight,
        orthoHalfHeight,
        nearPlane,
        farPlane);
    lighting.shadowViewProj = lightProj * lightView;
    return lighting;
}

SceneLighting buildSceneLighting(
    const SceneView& sceneView,
    const SceneSnapshot& snapshot,
    const int shadowMapResolution,
    const BackdropPreset backdropPreset)
{
    return buildSceneLighting(sceneView, snapshot, {}, shadowMapResolution, backdropPreset);
}

} // namespace render_scene

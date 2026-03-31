#include "render/SceneRenderer.h"
#include "render/SceneLighting.h"
#include "render/RenderMaterial.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstddef>
namespace render_scene {

namespace {

[[nodiscard]] bool ndcInClipVolume(const glm::vec3& ndc)
{
    return ndc.x >= -1.0f && ndc.x <= 1.0f &&
        ndc.y >= -1.0f && ndc.y <= 1.0f &&
        ndc.z >= -1.0f && ndc.z <= 1.0f;
}

std::array<float, 4> pathTrailColor(const int pathColorIndex, const bool simFrozen)
{
    constexpr std::array<std::array<float, 3>, 6> kBaseColors{{
        {{0.37f, 0.81f, 0.97f}},
        {{0.96f, 0.70f, 0.30f}},
        {{0.92f, 0.94f, 0.98f}},
        {{0.56f, 0.91f, 0.43f}},
        {{0.92f, 0.50f, 0.86f}},
        {{0.94f, 0.39f, 0.39f}},
    }};
    const std::size_t colorIndex = static_cast<std::size_t>(
        std::clamp(pathColorIndex, 0, static_cast<int>(kBaseColors.size()) - 1));
    std::array<float, 4> color{
        kBaseColors[colorIndex][0],
        kBaseColors[colorIndex][1],
        kBaseColors[colorIndex][2],
        simFrozen ? 0.56f : 0.75f,
    };
    if (simFrozen) {
        color[0] = std::min(1.0f, color[0] * 0.72f + 0.28f);
        color[1] = std::min(1.0f, color[1] * 0.72f + 0.28f);
        color[2] = std::min(1.0f, color[2] * 0.72f + 0.28f);
    }
    return color;
}

} // namespace

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

bool worldToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    const int fbw,
    const int fbh,
    float& outXPx,
    float& outYPx)
{
    const glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (!ndcInClipVolume(ndc)) {
        return false;
    }

    outXPx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(fbw);
    outYPx = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(fbh);
    return true;
}

SceneView buildSceneView(
    const input::Camera& cam,
    const ui::CameraSettings& cameraSettings,
    const FramebufferSize& framebufferSize)
{
    const float aspect = framebufferSize.h > 0
        ? static_cast<float>(framebufferSize.w) / static_cast<float>(framebufferSize.h)
        : 1.0f;

    SceneView sceneView;
    sceneView.cameraPosition = cam.pos;
    sceneView.forward = input::forwardDir(cam);
    sceneView.nearPlane = 0.1f;
    sceneView.farPlane = 6000.0f;
    sceneView.proj = glm::perspective(
        glm::radians(std::clamp(cameraSettings.fovDegrees, 30.0f, 130.0f)),
        aspect,
        sceneView.nearPlane,
        sceneView.farPlane);
    sceneView.view = glm::lookAt(cam.pos, cam.pos + sceneView.forward, glm::vec3(0.0f, 1.0f, 0.0f));
    sceneView.viewProj = sceneView.proj * sceneView.view;
    return sceneView;
}

void buildSceneInstances(
    std::vector<SceneInstanceData>& instanceScratch,
    const SceneSnapshot& snapshot)
{
    const std::size_t instanceCount = std::min(snapshot.models.size(), snapshot.bodies.size());
    instanceScratch.resize(instanceCount);
    for (std::size_t i = 0; i < instanceCount; ++i) {
        const RenderMaterial material = resolvePresentationMaterial(RenderMaterialInputs{
            snapshot.bodies[i].materialName,
            snapshot.bodies[i].radius,
            snapshot.bodies[i].invMass,
            snapshot.bodies[i].bodyId,
        });
        instanceScratch[i] = SceneInstanceData{
            snapshot.models[i],
            glm::vec4(material.albedo, material.roughness),
            glm::vec4(material.metalness, 0.0f, 0.0f, 0.0f),
            glm::vec4(material.emissiveColor, material.emissiveIntensity),
            glm::vec4(
                static_cast<float>(material.surfacePattern),
                material.detailScale,
                material.detailStrength,
                material.surfaceSeed),
        };
    }
}

void uploadSceneInstances(
    const GLuint instanceVbo,
    InstanceBufferState& instanceBufferState,
    const std::span<const SceneInstanceData> instances)
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    if (instances.size() > instanceBufferState.capacity) {
        instanceBufferState.capacity = instances.size();
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(instanceBufferState.capacity * sizeof(SceneInstanceData)),
            nullptr,
            GL_DYNAMIC_DRAW);
    }
    if (!instances.empty()) {
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(instances.size() * sizeof(SceneInstanceData)),
            instances.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawBodies(
    const ScenePassResources& resources,
    const SceneView& sceneView,
    const SceneLighting& lighting,
    const std::span<const SceneInstanceData> instances)
{
    if (instances.empty()) {
        return;
    }

    glUseProgram(resources.program);
    glUniformMatrix4fv(resources.uView, 1, GL_FALSE, &sceneView.view[0][0]);
    glUniformMatrix4fv(resources.uProj, 1, GL_FALSE, &sceneView.proj[0][0]);
    glUniform3fv(resources.uCameraPos, 1, &sceneView.cameraPosition[0]);
    glUniform3fv(resources.uLightDir, 1, &lighting.direction[0]);
    glUniform3fv(resources.uLightColor, 1, &lighting.color[0]);
    glUniform3fv(resources.uSkyZenithColor, 1, &lighting.skyZenithColor[0]);
    glUniform3fv(resources.uSkyHorizonColor, 1, &lighting.skyHorizonColor[0]);
    glUniform3fv(resources.uGroundColor, 1, &lighting.groundColor[0]);
    glUniformMatrix4fv(resources.uLightViewProj, 1, GL_FALSE, &lighting.shadowViewProj[0][0]);
    glUniform2fv(resources.uShadowTexelSize, 1, &lighting.shadowTexelSize[0]);
    glUniform1i(resources.uLocalLightCount, lighting.localLightCount);
    if (lighting.localLightCount > 0) {
        std::array<glm::vec4, kMaxLocalLights> localLightPosRange{};
        std::array<glm::vec4, kMaxLocalLights> localLightColorIntensity{};
        for (int i = 0; i < lighting.localLightCount; ++i) {
            const LocalLight& light = lighting.localLights[static_cast<std::size_t>(i)];
            localLightPosRange[static_cast<std::size_t>(i)] = glm::vec4(light.position, light.range);
            localLightColorIntensity[static_cast<std::size_t>(i)] = glm::vec4(light.color, light.intensity);
        }
        glUniform4fv(
            resources.uLocalLightPosRange,
            lighting.localLightCount,
            &localLightPosRange[0].x);
        glUniform4fv(
            resources.uLocalLightColorIntensity,
            lighting.localLightCount,
            &localLightColorIntensity[0].x);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resources.shadowTexture);

    glBindVertexArray(resources.sphereVao);
    glDrawElementsInstanced(
        GL_TRIANGLES,
        resources.sphereIndexCount,
        GL_UNSIGNED_INT,
        nullptr,
        static_cast<GLsizei>(instances.size()));
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void drawPathTrails(
    const GLuint pathVao,
    const GLuint pathVbo,
    const GLuint program,
    const GLint uViewProj,
    const GLint uColor,
    const SceneView& sceneView,
    PathBufferState& pathBufferState,
    std::vector<glm::vec3>& scratchPoints,
    std::vector<GLint>& drawStarts,
    std::vector<GLsizei>& drawCounts,
    const std::span<const PathTrail> pathTrails,
    const std::uint64_t pathTrailsRevision,
    const int pathColorIndex,
    const bool simFrozen)
{
    if (pathBufferState.uploadedRevision != pathTrailsRevision) {
        scratchPoints.clear();
        drawStarts.clear();
        drawCounts.clear();

        std::size_t totalPointCount = 0;
        for (const PathTrail& trail : pathTrails) {
            if (trail.count >= 2 && !trail.points.empty()) {
                totalPointCount += trail.count;
            }
        }
        scratchPoints.reserve(totalPointCount);
        drawStarts.reserve(pathTrails.size());
        drawCounts.reserve(pathTrails.size());

        for (const PathTrail& trail : pathTrails) {
            if (trail.count < 2 || trail.points.empty()) {
                continue;
            }

            drawStarts.push_back(static_cast<GLint>(scratchPoints.size()));
            drawCounts.push_back(static_cast<GLsizei>(trail.count));
            for (std::size_t i = 0; i < trail.count; ++i) {
                const std::size_t index = (trail.start + i) % trail.points.size();
                const sim::Vec3& point = trail.points[index];
                scratchPoints.emplace_back(
                    static_cast<float>(point.x),
                    static_cast<float>(point.y),
                    static_cast<float>(point.z));
            }
        }

        if (scratchPoints.empty()) {
            pathBufferState.uploadedRevision = pathTrailsRevision;
            return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, pathVbo);
        if (scratchPoints.size() > pathBufferState.capacity) {
            pathBufferState.capacity = scratchPoints.size();
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(pathBufferState.capacity * sizeof(glm::vec3)),
                nullptr,
                GL_DYNAMIC_DRAW);
        }
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(scratchPoints.size() * sizeof(glm::vec3)),
            scratchPoints.data());
        pathBufferState.uploadedRevision = pathTrailsRevision;
    }

    if (drawStarts.empty()) {
        return;
    }

    glUseProgram(program);
    glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &sceneView.viewProj[0][0]);
    const auto color = pathTrailColor(pathColorIndex, simFrozen);
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);

    glBindVertexArray(pathVao);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glMultiDrawArrays(
        GL_LINE_STRIP,
        drawStarts.data(),
        drawCounts.data(),
        static_cast<GLsizei>(drawStarts.size()));
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

} // namespace render_scene

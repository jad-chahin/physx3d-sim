#include "render/SceneRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
namespace render_scene {

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
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
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
    sceneView.forward = input::forwardDir(cam);
    sceneView.proj = glm::perspective(
        glm::radians(std::clamp(cameraSettings.fovDegrees, 30.0f, 130.0f)),
        aspect,
        0.1f,
        1000.0f);
    sceneView.view = glm::lookAt(cam.pos, cam.pos + sceneView.forward, glm::vec3(0.0f, 1.0f, 0.0f));
    sceneView.viewProj = sceneView.proj * sceneView.view;
    return sceneView;
}

void drawBodies(
    const GLuint instanceVbo,
    const GLuint program,
    const GLint uView,
    const GLint uProj,
    const GLint uLightDir,
    const GLint uBaseColor,
    const GLint uAmbient,
    const GLuint sphereVao,
    const GLsizei sphereIndexCount,
    const SceneView& sceneView,
    InstanceBufferState& instanceBufferState,
    const SceneSnapshot& snapshot)
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    if (snapshot.models.size() > instanceBufferState.capacity) {
        instanceBufferState.capacity = snapshot.models.size();
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(instanceBufferState.capacity * sizeof(glm::mat4)),
            nullptr,
            GL_DYNAMIC_DRAW);
    }
    if (!snapshot.models.empty()) {
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(snapshot.models.size() * sizeof(glm::mat4)),
            snapshot.models.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(program);
    glUniformMatrix4fv(uView, 1, GL_FALSE, &sceneView.view[0][0]);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, &sceneView.proj[0][0]);
    glUniform3f(uLightDir, 0.4f, 0.7f, 0.6f);
    glUniform3f(uBaseColor, 1.0f, 1.0f, 0.0f);
    glUniform1f(uAmbient, 0.25f);

    glBindVertexArray(sphereVao);
    glDrawElementsInstanced(
        GL_TRIANGLES,
        sphereIndexCount,
        GL_UNSIGNED_INT,
        nullptr,
        static_cast<GLsizei>(snapshot.models.size()));
    glBindVertexArray(0);
}

} // namespace render_scene

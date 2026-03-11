#include "app/AppLoopInternal.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace app_loop {

GlfwSession::~GlfwSession() {
    if (active_) {
        glfwTerminate();
    }
}

bool GlfwSession::init() {
    active_ = glfwInit() == GLFW_TRUE;
    return active_;
}

WindowHandle::~WindowHandle() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
}

void WindowHandle::reset(GLFWwindow* window) {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    window_ = window;
}

GLFWwindow* WindowHandle::get() const {
    return window_;
}

WindowHandle::operator GLFWwindow*() const {
    return window_;
}

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

double consumeScrollDeltaY(AppLoopState& state) {
    const double value = state.scrollDeltaY;
    state.scrollDeltaY = 0.0;
    return value;
}

int consumeLastPressedKey(AppLoopState& state) {
    const int key = state.lastPressedKey;
    state.lastPressedKey = GLFW_KEY_UNKNOWN;
    return key;
}

void applySimulationSettings(sim::World& world, const ui::SimulationSettings& simSettings) {
    world.params().enableGravity = simSettings.gravityEnabled;
    world.params().G = simSettings.gravityStrength;
    world.params().collisionIterations = simSettings.collisionIterations;
}

void updateFps(RuntimeState& runtime, const double frameTime) {
    runtime.fpsElapsed += frameTime;
    ++runtime.fpsFrames;
    if (runtime.fpsElapsed >= kFpsUpdateInterval) {
        runtime.hudFps = static_cast<double>(runtime.fpsFrames) / runtime.fpsElapsed;
        runtime.fpsElapsed = 0.0;
        runtime.fpsFrames = 0;
    }
}

void syncPreviousPositions(sim::World& world) {
    for (auto& body : world.bodies()) {
        body.prevPosition = body.position;
    }
}

void handleSimulationHotkeys(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::SimulationSettings& simSettings,
    const bool pauseMenuOpen,
    RuntimeState& runtime,
    sim::World& world)
{
    const bool freezeDown = input::isBindingPressed(window, controls.freeze);
    if (!pauseMenuOpen && freezeDown && !runtime.freezeWasDown) {
        runtime.simFrozen = !runtime.simFrozen;
        runtime.lastTime = glfwGetTime();
        runtime.accumulator = 0.0;
        syncPreviousPositions(world);
    }
    runtime.freezeWasDown = freezeDown;

    const bool speedDown = input::isBindingPressed(window, controls.speedDown);
    if (!pauseMenuOpen && speedDown && !runtime.speedDownWasDown) {
        runtime.simSpeed = std::max(simSettings.minSimSpeed, runtime.simSpeed * 0.5);
    }
    runtime.speedDownWasDown = speedDown;

    const bool speedUp = input::isBindingPressed(window, controls.speedUp);
    if (!pauseMenuOpen && speedUp && !runtime.speedUpWasDown) {
        runtime.simSpeed = std::min(simSettings.maxSimSpeed, runtime.simSpeed * 2.0);
    }
    runtime.speedUpWasDown = speedUp;

    const bool speedReset = input::isBindingPressed(window, controls.speedReset);
    if (!pauseMenuOpen && speedReset && !runtime.speedResetWasDown) {
        runtime.simSpeed = std::clamp(1.0, simSettings.minSimSpeed, simSettings.maxSimSpeed);
    }
    runtime.speedResetWasDown = speedReset;
}

void updateCamera(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    const double frameTime,
    input::Camera& cam)
{
    if (!runtime.mouseCaptured) {
        return;
    }

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    if (runtime.firstMouse) {
        runtime.lastMouseX = mx;
        runtime.lastMouseY = my;
        runtime.firstMouse = false;
    }

    const double dx = mx - runtime.lastMouseX;
    const double dy = my - runtime.lastMouseY;
    runtime.lastMouseX = mx;
    runtime.lastMouseY = my;

    cam.yaw += static_cast<float>(dx) * cameraSettings.lookSensitivity;
    cam.pitch += static_cast<float>(cameraSettings.invertY ? dy : -dy) * cameraSettings.lookSensitivity;
    input::clampPitch(cam);

    float move = cameraSettings.baseMoveSpeed * static_cast<float>(frameTime);
    if (input::isBindingPressed(window, controls.cameraBoost)) {
        constexpr float kCameraBoostMultiplier = 4.0f;
        move *= kCameraBoostMultiplier;
    }

    if (input::isBindingPressed(window, controls.moveForward)) cam.pos += input::forwardDir(cam) * move;
    if (input::isBindingPressed(window, controls.moveBack)) cam.pos -= input::forwardDir(cam) * move;
    if (input::isBindingPressed(window, controls.moveRight)) cam.pos += input::rightDir(cam) * move;
    if (input::isBindingPressed(window, controls.moveLeft)) cam.pos -= input::rightDir(cam) * move;
    if (input::isBindingPressed(window, controls.moveUp)) cam.pos += glm::vec3(0, 1, 0) * move;
    if (input::isBindingPressed(window, controls.moveDown)) cam.pos -= glm::vec3(0, 1, 0) * move;
}

void stepSimulation(sim::World& world, const double frameTime, RuntimeState& runtime, const bool pauseMenuOpen) {
    if (runtime.simFrozen || pauseMenuOpen) {
        return;
    }

    const double simFrameTime =
        std::min(frameTime, static_cast<double>(kInternalMaxPhysicsStepsPerFrame) * kFixedDt);
    runtime.accumulator += simFrameTime * runtime.simSpeed;

    int simStepsThisFrame = 0;
    while (runtime.accumulator >= kFixedDt && simStepsThisFrame < kInternalMaxPhysicsStepsPerFrame) {
        syncPreviousPositions(world);
        world.step(kFixedDt);
        runtime.accumulator -= kFixedDt;
        ++simStepsThisFrame;
    }

    constexpr double maxCarryOver = static_cast<double>(kInternalMaxPhysicsStepsPerFrame) * kFixedDt;
    runtime.accumulator = std::min(runtime.accumulator, maxCarryOver);
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
    return sceneView;
}

void buildRenderModels(
    const sim::World& world,
    const double alpha,
    std::vector<glm::mat4>& models,
    std::vector<glm::vec3>& renderPositions)
{
    const auto& bodies = world.bodies();
    models.resize(bodies.size());
    renderPositions.resize(bodies.size());

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const sim::Vec3 interpolatedPosition =
            bodies[i].prevPosition + (bodies[i].position - bodies[i].prevPosition) * alpha;
        const glm::vec3 pos(
            static_cast<float>(interpolatedPosition.x),
            static_cast<float>(interpolatedPosition.y),
            static_cast<float>(interpolatedPosition.z));
        const auto radius = static_cast<float>(bodies[i].radius);

        renderPositions[i] = pos;
        models[i] = glm::scale(glm::translate(glm::mat4(1.0f), pos), glm::vec3(radius, radius, radius));
    }
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
    const std::vector<glm::mat4>& models)
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    if (models.size() > instanceBufferState.capacity) {
        instanceBufferState.capacity = models.size();
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(instanceBufferState.capacity * sizeof(glm::mat4)),
            nullptr,
            GL_DYNAMIC_DRAW);
    }
    if (!models.empty()) {
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(models.size() * sizeof(glm::mat4)),
            models.data());
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
        static_cast<GLsizei>(models.size()));
    glBindVertexArray(0);
}

OverlayRenderer::TargetHud buildTargetHud(
    const sim::World& world,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const SceneView& sceneView,
    const FramebufferSize& framebufferSize,
    const std::vector<glm::vec3>& renderPositions)
{
    OverlayRenderer::TargetHud targetHud{};
    if (!interfaceSettings.objectInfo || renderPositions.empty()) {
        return targetHud;
    }

    const auto& bodies = world.bodies();
    const glm::vec3 rayOrigin = cam.pos;
    const glm::vec3 rayDir = glm::normalize(sceneView.forward);

    int bestIdx = -1;
    float bestT = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < renderPositions.size(); ++i) {
        float t = 0.0f;
        if (!raySphereHit(rayOrigin, rayDir, renderPositions[i], static_cast<float>(bodies[i].radius), t)) {
            continue;
        }
        if (t < bestT) {
            bestT = t;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestIdx < 0) {
        return targetHud;
    }

    const auto i = static_cast<std::size_t>(bestIdx);
    const glm::vec3 labelPos =
        renderPositions[i] + glm::vec3(0.0f, static_cast<float>(bodies[i].radius) * 1.6f, 0.0f);
    float sx = 0.0f;
    float sy = 0.0f;
    if (!worldToScreen(labelPos, sceneView.view, sceneView.proj, framebufferSize.w, framebufferSize.h, sx, sy)) {
        return targetHud;
    }

    targetHud.visible = true;
    targetHud.xPx = sx;
    targetHud.yPx = sy;
    targetHud.lines.emplace_back("TARGET");

    char line[64];
    if (interfaceSettings.objectInfoMaterial) {
        std::snprintf(line, sizeof(line), "E:%.2f", bodies[i].restitution);
        targetHud.lines.emplace_back(line);
        std::snprintf(line, sizeof(line), "S:%.2f", bodies[i].staticFriction);
        targetHud.lines.emplace_back(line);
        std::snprintf(line, sizeof(line), "D:%.2f", bodies[i].dynamicFriction);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoVelocity) {
        std::snprintf(line, sizeof(line), "V:%.2f", bodies[i].velocity.magnitude());
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoRadius) {
        std::snprintf(line, sizeof(line), "R:%.2f", bodies[i].radius);
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoMass) {
        if (bodies[i].invMass > 0.0) {
            std::snprintf(line, sizeof(line), "M:%.2f", 1.0 / bodies[i].invMass);
        } else {
            std::snprintf(line, sizeof(line), "M:INF");
        }
        targetHud.lines.emplace_back(line);
    }

    return targetHud;
}

void drawOverlay(
    const OverlayRenderer& overlay,
    const FramebufferSize& framebufferSize,
    const RuntimeState& runtime,
    const OverlayRenderer::PauseMenuHud& pauseMenuHud,
    const OverlayRenderer::TargetHud& targetHud,
    const float uiScale,
    const ui::InterfaceSettings& interfaceSettings)
{
    static constexpr std::vector<std::string> kNoHudDebugLines;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    overlay.draw(
        framebufferSize.w,
        framebufferSize.h,
        runtime.simFrozen,
        runtime.simSpeed,
        runtime.hudFps,
        pauseMenuHud,
        targetHud,
        uiScale,
        interfaceSettings.showHud,
        interfaceSettings.showCrosshair,
        kNoHudDebugLines);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace app_loop

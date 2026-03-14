#include "app/AppLoopInternal.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace app_loop {
namespace {
    constexpr double kHudMetricsUpdateInterval = 0.5;
    constexpr std::size_t kMaxPathPoints = 128;

    void accumulateHudMetrics(RuntimeState& runtime, const sim::World::Metrics& metrics) {
        auto& accum = runtime.hudMetrics.accumulated;
        accum.broadphaseCandidatesDiscrete += metrics.broadphaseCandidatesDiscrete;
        accum.broadphaseCandidatesSwept += metrics.broadphaseCandidatesSwept;
        accum.pairsVisited += metrics.pairsVisited;
        accum.pairsImpulseApplied += metrics.pairsImpulseApplied;
        accum.ccdEvents += metrics.ccdEvents;
        accum.ccdZeroTimePairsResolved += metrics.ccdZeroTimePairsResolved;
        accum.warmStartedPairs += metrics.warmStartedPairs;
        accum.manifoldActivePairs += metrics.manifoldActivePairs;
        accum.sanitizedBodies += metrics.sanitizedBodies;
        accum.sanitizedFields += metrics.sanitizedFields;
        runtime.hudMetrics.elapsed += kFixedDt;
        ++runtime.hudMetrics.samples;
    }

    std::size_t averageMetric(const std::size_t total, const std::size_t samples) {
        if (samples == 0) {
            return 0;
        }
        return (total + samples / 2) / samples;
    }

    void refreshDisplayedHudMetrics(RuntimeState& runtime) {
        if (runtime.hudMetrics.samples == 0 || runtime.hudMetrics.elapsed < kHudMetricsUpdateInterval) {
            return;
        }

        const auto& accum = runtime.hudMetrics.accumulated;
        auto& displayed = runtime.hudMetrics.displayed;
        const std::size_t samples = runtime.hudMetrics.samples;

        displayed.broadphaseCandidatesDiscrete = averageMetric(accum.broadphaseCandidatesDiscrete, samples);
        displayed.broadphaseCandidatesSwept = averageMetric(accum.broadphaseCandidatesSwept, samples);
        displayed.pairsVisited = averageMetric(accum.pairsVisited, samples);
        displayed.pairsImpulseApplied = averageMetric(accum.pairsImpulseApplied, samples);
        displayed.ccdEvents = averageMetric(accum.ccdEvents, samples);
        displayed.ccdZeroTimePairsResolved = averageMetric(accum.ccdZeroTimePairsResolved, samples);
        displayed.warmStartedPairs = averageMetric(accum.warmStartedPairs, samples);
        displayed.manifoldActivePairs = averageMetric(accum.manifoldActivePairs, samples);
        displayed.sanitizedBodies = averageMetric(accum.sanitizedBodies, samples);
        displayed.sanitizedFields = averageMetric(accum.sanitizedFields, samples);

        runtime.hudMetrics.accumulated = {};
        runtime.hudMetrics.elapsed = 0.0;
        runtime.hudMetrics.samples = 0;
    }

    void pushPathPoint(RuntimeState::PathTrail& trail, const sim::Vec3& position) {
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
        const sim::World& world,
        const input::Camera& cam,
        const SceneView& sceneView,
        const std::vector<glm::vec3>& renderPositions)
    {
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
        return bestIdx;
    }
}

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
    world.params().enableCollisions = simSettings.collisionsEnabled;
    world.params().collisionIterations = simSettings.collisionIterations;
    world.params().restitution = simSettings.globalRestitution;
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
}

void syncPreviousState(sim::World& world) {
    for (auto& body : world.bodies()) {
        body.prevPosition = body.position;
        body.prevOrientation = body.orientation;
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
        syncPreviousState(world);
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
        syncPreviousState(world);
        world.step(kFixedDt);
        accumulateHudMetrics(runtime, world.metrics());
        runtime.accumulator -= kFixedDt;
        ++simStepsThisFrame;
    }

    constexpr double maxCarryOver = static_cast<double>(kInternalMaxPhysicsStepsPerFrame) * kFixedDt;
    runtime.accumulator = std::min(runtime.accumulator, maxCarryOver);
    refreshDisplayedHudMetrics(runtime);
}

void reloadDefaultWorld(
    sim::World& world,
    RuntimeState& runtime,
    const ui::SimulationSettings& simSettings)
{
    world = sim::makeDefaultWorld();
    applySimulationSettings(world, simSettings);
    runtime.accumulator = 0.0;
    runtime.lastTime = glfwGetTime();
    runtime.fpsElapsed = 0.0;
    runtime.fpsFrames = 0;
    runtime.freezeWasDown = false;
    runtime.speedDownWasDown = false;
    runtime.speedUpWasDown = false;
    runtime.speedResetWasDown = false;
    runtime.hudMetrics = {};
    runtime.pathHistory.clear();
    syncPreviousState(world);
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
        const sim::Quaternion interpolatedOrientation =
            sim::nlerpQuat(bodies[i].prevOrientation, bodies[i].orientation, alpha);
        const glm::vec3 pos(
            static_cast<float>(interpolatedPosition.x),
            static_cast<float>(interpolatedPosition.y),
            static_cast<float>(interpolatedPosition.z));
        const float radius = static_cast<float>(bodies[i].radius);

        renderPositions[i] = pos;

        glm::mat4 model(1.0f);
        const glm::mat3 rotation = quatToMat3(interpolatedOrientation);
        model[0] = glm::vec4(rotation[0] * radius, 0.0f);
        model[1] = glm::vec4(rotation[1] * radius, 0.0f);
        model[2] = glm::vec4(rotation[2] * radius, 0.0f);
        model[3][0] = pos.x;
        model[3][1] = pos.y;
        model[3][2] = pos.z;
        models[i] = model;
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
    const int bestIdx = findTargetBodyIndex(world, cam, sceneView, renderPositions);

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
    targetHud.lines.reserve(6);

    char line[64];
    if (interfaceSettings.objectInfoMaterial) {
        std::snprintf(line, sizeof(line), "MAT:%s", bodies[i].materialName.c_str());
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoVelocity) {
        std::snprintf(line, sizeof(line), "V:%.2f", bodies[i].velocity.magnitude());
        targetHud.lines.emplace_back(line);
    }
    if (interfaceSettings.objectInfoAngularSpeed) {
        std::snprintf(line, sizeof(line), "A:%.2f", bodies[i].angularVelocity.magnitude());
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
    if (interfaceSettings.objectInfoBodyType) {
        std::snprintf(line, sizeof(line), "T:%s", bodies[i].invMass > 0.0 ? "DYNAMIC" : "STATIC");
        targetHud.lines.emplace_back(line);
    }
    return targetHud;
}

std::vector<std::string> buildHudDebugLines(
    const RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings)
{
    if (!interfaceSettings.showPhysicsStats) {
        return {};
    }

    const sim::World::Metrics& metrics = runtime.hudMetrics.displayed;
    std::vector<std::string> lines;
    lines.reserve(4);

    char line[96];
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

    std::snprintf(
        line,
        sizeof(line),
        "CCD:%zu  ZERO:%zu",
        metrics.ccdEvents,
        metrics.ccdZeroTimePairsResolved);
    lines.emplace_back(line);

    std::snprintf(
        line,
        sizeof(line),
        "SAN B:%zu  F:%zu",
        metrics.sanitizedBodies,
        metrics.sanitizedFields);
    lines.emplace_back(line);

    return lines;
}

void updatePathHistory(
    const sim::World& world,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings)
{
    if (!interfaceSettings.drawPath) {
        runtime.pathHistory.clear();
        return;
    }

    const auto& bodies = world.bodies();
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

std::vector<OverlayRenderer::ScreenLine> buildPathLines(
    const RuntimeState& runtime,
    const SceneView& sceneView,
    const FramebufferSize& framebufferSize,
    const ui::InterfaceSettings& interfaceSettings)
{
    if (!interfaceSettings.drawPath) {
        return {};
    }

    std::vector<OverlayRenderer::ScreenLine> pathLines;
    pathLines.reserve(runtime.pathHistory.size() * (kMaxPathPoints - 1));
    for (const auto& history : runtime.pathHistory) {
        if (history.count < 2 || history.points.empty()) {
            continue;
        }
        for (std::size_t segment = 1; segment < history.count; ++segment) {
            const std::size_t idx0 = (history.start + segment - 1) % history.points.size();
            const std::size_t idx1 = (history.start + segment) % history.points.size();
            const sim::Vec3& p0 = history.points[idx0];
            const sim::Vec3& p1 = history.points[idx1];

            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;
            if (!worldToScreen(
                    glm::vec3(
                        static_cast<float>(p0.x),
                        static_cast<float>(p0.y),
                        static_cast<float>(p0.z)),
                    sceneView.view,
                    sceneView.proj,
                    framebufferSize.w,
                    framebufferSize.h,
                    x0,
                    y0) ||
                !worldToScreen(
                    glm::vec3(
                        static_cast<float>(p1.x),
                        static_cast<float>(p1.y),
                        static_cast<float>(p1.z)),
                    sceneView.view,
                    sceneView.proj,
                    framebufferSize.w,
                    framebufferSize.h,
                    x1,
                    y1))
            {
                continue;
            }
            pathLines.push_back({x0, y0, x1, y1});
        }
    }
    return pathLines;
}

void drawOverlay(
    const OverlayRenderer& overlay,
    const FramebufferSize& framebufferSize,
    const RuntimeState& runtime,
    const OverlayRenderer::PauseMenuHud& pauseMenuHud,
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
        runtime.simFrozen,
        runtime.simSpeed,
        runtime.hudFps,
        pauseMenuHud,
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

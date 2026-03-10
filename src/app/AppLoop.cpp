#include "app/AppLoop.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/ShaderUtil.h"
#include "render/SphereMesh.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenuController.h"


namespace {
    constexpr double kTickRate = 60.0;
    constexpr double kFixedDt = 1.0 / kTickRate;
    constexpr double kFpsUpdateInterval = 0.5;
    constexpr int kInternalMaxPhysicsStepsPerFrame = 512;

    struct FramebufferSize {
        int w = 0;
        int h = 0;
    };

    struct AppLoopState {
        FramebufferSize framebufferSize{};
        double scrollDeltaY = 0.0;
        int lastPressedKey = GLFW_KEY_UNKNOWN;
    };

    struct RuntimeState {
        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
        bool firstMouse = true;
        bool mouseCaptured = true;
        bool freezeWasDown = false;
        bool speedDownWasDown = false;
        bool speedUpWasDown = false;
        bool speedResetWasDown = false;
        bool simFrozen = false;
        double simSpeed = 1.0;
        double lastTime = 0.0;
        double accumulator = 0.0;
        double fpsElapsed = 0.0;
        double hudFps = 0.0;
        int fpsFrames = 0;
    };

    struct SceneView {
        glm::vec3 forward{0.0f, 0.0f, -1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
    };

    struct InstanceBufferState {
        std::size_t capacity = 0;
    };

    class GlfwSession {
    public:
        GlfwSession() = default;
        GlfwSession(const GlfwSession&) = delete;
        GlfwSession& operator=(const GlfwSession&) = delete;

        ~GlfwSession() {
            if (active_) {
                glfwTerminate();
            }
        }

        bool init() {
            active_ = glfwInit() == GLFW_TRUE;
            return active_;
        }

    private:
        bool active_ = false;
    };

    class WindowHandle {
    public:
        WindowHandle() = default;
        WindowHandle(const WindowHandle&) = delete;
        WindowHandle& operator=(const WindowHandle&) = delete;

        ~WindowHandle() {
            if (window_ != nullptr) {
                glfwDestroyWindow(window_);
            }
        }

        void reset(GLFWwindow* window) {
            if (window_ != nullptr) {
                glfwDestroyWindow(window_);
            }
            window_ = window;
        }

        GLFWwindow* get() const {
            return window_;
        }

        operator GLFWwindow*() const {
            return window_;
        }

    private:
        GLFWwindow* window_ = nullptr;
    };

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
        if (cameraSettings.invertY) {
            cam.pitch += static_cast<float>(dy) * cameraSettings.lookSensitivity;
        } else {
            cam.pitch -= static_cast<float>(dy) * cameraSettings.lookSensitivity;
        }
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
        if (runtime.accumulator > maxCarryOver) {
            runtime.accumulator = maxCarryOver;
        }
    }

    SceneView buildSceneView(
        const input::Camera& cam,
        const ui::CameraSettings& cameraSettings,
        const FramebufferSize& framebufferSize)
    {
        const float aspect = (framebufferSize.h > 0)
            ? (static_cast<float>(framebufferSize.w) / static_cast<float>(framebufferSize.h))
            : 1.0f;
        const float fovDegrees = std::clamp(cameraSettings.fovDegrees, 30.0f, 130.0f);

        SceneView sceneView;
        sceneView.forward = input::forwardDir(cam);
        sceneView.proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.1f, 1000.0f);
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
            const float radius = static_cast<float>(bodies[i].radius);

            renderPositions[i] = pos;

            glm::mat4 model(1.0f);
            model = glm::translate(model, pos);
            model = glm::scale(model, glm::vec3(radius, radius, radius));
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
        glDrawElementsInstanced(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(models.size()));
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
}

namespace app {
    int runApp(sim::World world) {
        GlfwSession glfwSession;
        if (!glfwSession.init()) {
            std::cerr << "Failed to init GLFW\n";
            return 1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        WindowHandle window;
        window.reset(glfwCreateWindow(1920, 1080, "physics3d", nullptr, nullptr));
        if (window.get() == nullptr) {
            std::cerr << "Failed to create GLFW window\n";
            return 1;
        }
        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;
        glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwMakeContextCurrent(window);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        AppLoopState appState{};
        glfwSetWindowUserPointer(window, &appState);

        input::Camera cam{};
        RuntimeState runtime{};

        input::ControlBindings controls{};
        const std::string controlsConfigPath = "controls.cfg";
        input::loadControlBindings(controls, controlsConfigPath);

        ui::PauseMenuController pauseMenu;
        const std::string settingsConfigPath = "settings.cfg";
        pauseMenu.loadSettings(settingsConfigPath);
        pauseMenu.applyCurrentDisplaySettings(window);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "Failed to initialize GLAD\n";
            return 1;
        }

        std::cout << "Vendor:   " << glGetString(GL_VENDOR) << "\n";
        std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
        std::cout << "Version:  " << glGetString(GL_VERSION) << "\n";

        int flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugCallback, nullptr);
        }

        glfwGetFramebufferSize(window, &appState.framebufferSize.w, &appState.framebufferSize.h);
        glViewport(0, 0, appState.framebufferSize.w, appState.framebufferSize.h);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* callbackWindow, const int w, const int h) {
            auto* state = static_cast<AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
            if (state != nullptr) {
                state->framebufferSize.w = w;
                state->framebufferSize.h = h;
            }
            glViewport(0, 0, w, h);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* callbackWindow, const int key, const int scancode, const int action, const int mods) {
            (void)scancode;
            (void)mods;
            auto* state = static_cast<AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
            if (state != nullptr && action == GLFW_PRESS && key != GLFW_KEY_UNKNOWN) {
                state->lastPressedKey = key;
            }
        });
        glfwSetScrollCallback(window, [](GLFWwindow* callbackWindow, const double xoffset, const double yoffset) {
            auto* state = static_cast<AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
            (void)xoffset;
            if (state != nullptr) {
                state->scrollDeltaY += yoffset;
            }
        });

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        std::vector<float> sphereVerts;
        std::vector<std::uint32_t> sphereIdx;
        buildSphereMesh(16, 32, sphereVerts, sphereIdx);
        const auto sphereIndexCount = static_cast<GLsizei>(sphereIdx.size());

        const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
        const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
        if (vs == 0 || fs == 0) {
            std::cerr << "Shader compile failed; exiting.\n";
            return 1;
        }

        const GLuint program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (program == 0) {
            std::cerr << "Program link failed; exiting.\n";
            return 1;
        }

        const GLint uView = glGetUniformLocation(program, "uView");
        const GLint uProj = glGetUniformLocation(program, "uProj");
        const GLint uLightDir = glGetUniformLocation(program, "uLightDir");
        const GLint uBaseColor = glGetUniformLocation(program, "uBaseColor");
        const GLint uAmbient = glGetUniformLocation(program, "uAmbient");
        if (uView < 0 || uProj < 0 || uLightDir < 0 || uBaseColor < 0 || uAmbient < 0) {
            std::cerr << "Missing uniform(s). Check shader names match exactly.\n";
        }

        GLuint sphereVao = 0;
        GLuint sphereVbo = 0;
        GLuint sphereEbo = 0;
        GLuint instanceVbo = 0;
        glGenVertexArrays(1, &sphereVao);
        glGenBuffers(1, &sphereVbo);
        glGenBuffers(1, &sphereEbo);
        glGenBuffers(1, &instanceVbo);

        glBindVertexArray(sphereVao);

        glBindBuffer(GL_ARRAY_BUFFER, sphereVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sphereVerts.size() * sizeof(float)),
            sphereVerts.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEbo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sphereIdx.size() * sizeof(std::uint32_t)),
            sphereIdx.data(),
            GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        constexpr std::size_t vec4Size = 4 * sizeof(float);
        constexpr std::size_t mat4Size = 4 * vec4Size;

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(mat4Size), reinterpret_cast<void*>(0 * vec4Size));
        glVertexAttribDivisor(2, 1);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(mat4Size), reinterpret_cast<void*>(1 * vec4Size));
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(mat4Size), reinterpret_cast<void*>(2 * vec4Size));
        glVertexAttribDivisor(4, 1);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(mat4Size), reinterpret_cast<void*>(3 * vec4Size));
        glVertexAttribDivisor(5, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        OverlayRenderer overlay;
        overlay.init();

        runtime.lastTime = glfwGetTime();

        std::vector<glm::mat4> models;
        std::vector<glm::vec3> renderPositions;
        InstanceBufferState instanceBufferState{};

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            const int pressedKey = consumeLastPressedKey(appState);
            const float scrollDeltaY = static_cast<float>(consumeScrollDeltaY(appState));
            auto& bs = world.bodies();

            pauseMenu.updateEscapeState(
                window,
                runtime.mouseCaptured,
                runtime.firstMouse,
                runtime.lastTime,
                runtime.accumulator,
                bs);
            pauseMenu.handlePointerInput(window, controls, controlsConfigPath, scrollDeltaY);
            pauseMenu.handlePressedKey(window, pressedKey, controls, controlsConfigPath);
            pauseMenu.updateContinuousInput(window, controls, controlsConfigPath);
            const auto& simSettings = pauseMenu.simulationSettings();
            const auto& cameraSettings = pauseMenu.cameraSettings();
            const auto& interfaceSettings = pauseMenu.interfaceSettings();
            applySimulationSettings(world, simSettings);
            runtime.simSpeed = std::clamp(runtime.simSpeed, simSettings.minSimSpeed, simSettings.maxSimSpeed);
            handleSimulationHotkeys(window, controls, simSettings, pauseMenu.isOpen(), runtime, world);

            const double now = glfwGetTime();
            double frameTime = now - runtime.lastTime;
            runtime.lastTime = now;
            frameTime = std::min(frameTime, 0.25);
            updateFps(runtime, frameTime);
            updateCamera(window, controls, cameraSettings, runtime, frameTime, cam);
            stepSimulation(world, frameTime, runtime, pauseMenu.isOpen());

            const double alpha = std::clamp(runtime.accumulator / kFixedDt, 0.0, 1.0);
            if (runtime.simFrozen && !pauseMenu.isOpen()) {
                glClearColor(0.20f, 0.02f, 0.02f, 1.0f);
            } else {
                glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const OverlayRenderer::PauseMenuHud pauseMenuHud = pauseMenu.buildHud(controls);
            OverlayRenderer::TargetHud targetHud{};
            if (!bs.empty()) {
                const SceneView sceneView = buildSceneView(cam, cameraSettings, appState.framebufferSize);
                buildRenderModels(world, alpha, models, renderPositions);
                drawBodies(
                    instanceVbo,
                    program,
                    uView,
                    uProj,
                    uLightDir,
                    uBaseColor,
                    uAmbient,
                    sphereVao,
                    sphereIndexCount,
                    sceneView,
                    instanceBufferState,
                    models);
                targetHud = buildTargetHud(
                    world,
                    interfaceSettings,
                    cam,
                    sceneView,
                    appState.framebufferSize,
                    renderPositions);
            }

            drawOverlay(
                overlay,
                appState.framebufferSize,
                runtime,
                pauseMenuHud,
                targetHud,
                pauseMenu.uiScale(),
                interfaceSettings);

            glfwSwapBuffers(window);
        }

        glDeleteBuffers(1, &instanceVbo);
        glDeleteBuffers(1, &sphereEbo);
        glDeleteBuffers(1, &sphereVbo);
        glDeleteVertexArrays(1, &sphereVao);
        glDeleteProgram(program);
        overlay.shutdown();

        return 0;
    }
} // namespace app

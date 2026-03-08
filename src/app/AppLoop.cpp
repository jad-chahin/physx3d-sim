//
// Created by Codex on 2026-03-07.
//

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
#include "ui/DebugOverlay.h"
#include "ui/PauseMenuController.h"


namespace {
    struct FramebufferSize {
        int w = 0;
        int h = 0;
    };
    FramebufferSize g_fb;

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
}

namespace app {
    int runApp(sim::World world) {
        if (!glfwInit()) {
            std::cerr << "Failed to init GLFW\n";
            return 1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        GLFWwindow* window = glfwCreateWindow(1920, 1080, "physics3d", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            return 1;
        }
        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;
        glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwMakeContextCurrent(window);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
        bool firstMouse = true;

        Camera cam{};

        bool mouseCaptured = true;
        bool spaceWasDown = false;
        bool minusWasDown = false;
        bool equalWasDown = false;
        bool oneWasDown = false;
        bool simFrozen = false;
        double simSpeed = 1.0;

        input::ControlBindings controls{};
        const std::string controlsConfigPath = "controls.cfg";
        input::loadControlBindings(controls, controlsConfigPath);

        ui::PauseMenuController pauseMenu;
        pauseMenu.applyCurrentDisplaySettings(window);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "Failed to initialize GLAD\n";
            glfwDestroyWindow(window);
            glfwTerminate();
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

        glfwGetFramebufferSize(window, &g_fb.w, &g_fb.h);
        glViewport(0, 0, g_fb.w, g_fb.h);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, const int w, const int h) {
            g_fb.w = w;
            g_fb.h = h;
            glViewport(0, 0, w, h);
        });
        glfwSetKeyCallback(window, input::keyCaptureCallback);

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
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        const GLuint program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (program == 0) {
            std::cerr << "Program link failed; exiting.\n";
            glfwDestroyWindow(window);
            glfwTerminate();
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

        DebugOverlay overlay;
        overlay.init();

        constexpr double tickRate = 60.0;
        constexpr double dt = 1.0 / tickRate;
        double lastTime = glfwGetTime();
        double accumulator = 0.0;
        double fpsElapsed = 0.0;
        double hudFps = 0.0;
        int fpsFrames = 0;
        constexpr double fpsUpdateInterval = 0.5;

        std::vector<glm::mat4> models;
        std::vector<glm::vec3> renderPositions;

        while (!glfwWindowShouldClose(window)) {
            constexpr std::vector<std::string> noHudDebugLines;
            glfwPollEvents();
            const int pressedKey = input::consumeLastPressedKey();
            auto& bs = world.bodies();

            pauseMenu.updateEscapeState(window, mouseCaptured, firstMouse, lastTime, accumulator, bs);
            pauseMenu.handlePointerInput(window, controls);
            pauseMenu.handlePressedKey(window, pressedKey, controls, controlsConfigPath);
            const auto& simSettings = pauseMenu.simulationSettings();
            const auto& cameraSettings = pauseMenu.cameraSettings();
            const auto& interfaceSettings = pauseMenu.interfaceSettings();
            simSpeed = std::clamp(simSpeed, simSettings.minSimSpeed, simSettings.maxSimSpeed);

            const bool spaceDown = (glfwGetKey(window, controls.freeze) == GLFW_PRESS);
            if (!pauseMenu.isOpen() && spaceDown && !spaceWasDown) {
                simFrozen = !simFrozen;
                lastTime = glfwGetTime();
                accumulator = 0.0;
                for (auto& body : bs) {
                    body.prevPosition = body.position;
                }
            }
            spaceWasDown = spaceDown;

            const bool minusDown = (glfwGetKey(window, controls.speedDown) == GLFW_PRESS);
            if (!pauseMenu.isOpen() && minusDown && !minusWasDown) {
                simSpeed = std::max(simSettings.minSimSpeed, simSpeed * 0.5);
            }
            minusWasDown = minusDown;

            const bool equalDown = (glfwGetKey(window, controls.speedUp) == GLFW_PRESS);
            if (!pauseMenu.isOpen() && equalDown && !equalWasDown) {
                simSpeed = std::min(simSettings.maxSimSpeed, simSpeed * 2.0);
            }
            equalWasDown = equalDown;

            const bool oneDown = (glfwGetKey(window, controls.speedReset) == GLFW_PRESS);
            if (!pauseMenu.isOpen() && oneDown && !oneWasDown) {
                simSpeed = std::clamp(1.0, simSettings.minSimSpeed, simSettings.maxSimSpeed);
            }
            oneWasDown = oneDown;

            const double now = glfwGetTime();
            double frameTime = now - lastTime;
            lastTime = now;
            frameTime = std::min(frameTime, 0.25);

            fpsElapsed += frameTime;
            ++fpsFrames;
            if (fpsElapsed >= fpsUpdateInterval) {
                hudFps = static_cast<double>(fpsFrames) / fpsElapsed;
                fpsElapsed = 0.0;
                fpsFrames = 0;
            }

            const int maxStepsPerFrame = std::max(1, simSettings.maxPhysicsStepsPerFrame);
            const double simFrameTime = std::min(frameTime, static_cast<double>(maxStepsPerFrame) * dt);

            if (mouseCaptured) {
                double mx = 0.0;
                double my = 0.0;
                glfwGetCursorPos(window, &mx, &my);
                if (firstMouse) {
                    lastMouseX = mx;
                    lastMouseY = my;
                    firstMouse = false;
                }
                const double dx = mx - lastMouseX;
                const double dy = my - lastMouseY;
                lastMouseX = mx;
                lastMouseY = my;

                cam.yaw += static_cast<float>(dx) * cameraSettings.lookSensitivity;
                if (cameraSettings.invertY) {
                    cam.pitch += static_cast<float>(dy) * cameraSettings.lookSensitivity;
                } else {
                    cam.pitch -= static_cast<float>(dy) * cameraSettings.lookSensitivity;
                }
                clampPitch(cam);

                float move = cameraSettings.baseMoveSpeed * static_cast<float>(frameTime);
                // Camera movement speed boost
                if (glfwGetKey(window, controls.cameraBoost) == GLFW_PRESS) {
                    constexpr float kCameraBoostMultiplier = 4.0f;
                    move *= kCameraBoostMultiplier;
                }

                if (glfwGetKey(window, controls.moveForward) == GLFW_PRESS) cam.pos += forwardDir(cam) * move;
                if (glfwGetKey(window, controls.moveBack) == GLFW_PRESS) cam.pos -= forwardDir(cam) * move;
                if (glfwGetKey(window, controls.moveRight) == GLFW_PRESS) cam.pos += rightDir(cam) * move;
                if (glfwGetKey(window, controls.moveLeft) == GLFW_PRESS) cam.pos -= rightDir(cam) * move;
                if (glfwGetKey(window, controls.moveUp) == GLFW_PRESS) cam.pos += glm::vec3(0, 1, 0) * move;
                if (glfwGetKey(window, controls.moveDown) == GLFW_PRESS) cam.pos -= glm::vec3(0, 1, 0) * move;
            }

            if (!simFrozen && !pauseMenu.isOpen()) {
                accumulator += simFrameTime * simSpeed;
                int simStepsThisFrame = 0;
                while (accumulator >= dt && simStepsThisFrame < maxStepsPerFrame) {
                    for (auto& body : bs) {
                        body.prevPosition = body.position;
                    }
                    world.step(dt);
                    accumulator -= dt;
                    ++simStepsThisFrame;
                }

                const double maxCarryOver = static_cast<double>(maxStepsPerFrame) * dt;
                if (accumulator > maxCarryOver) {
                    accumulator = maxCarryOver;
                }
            }

            const double alpha = std::clamp(accumulator / dt, 0.0, 1.0);
            const std::size_t n = bs.size();

            if (pauseMenu.isOpen()) {
                glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
            } else if (simFrozen) {
                glClearColor(0.20f, 0.02f, 0.02f, 1.0f);
            } else {
                glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (n == 0) {
                glfwSwapBuffers(window);
                continue;
            }

            const float aspect = (g_fb.h > 0) ? (static_cast<float>(g_fb.w) / static_cast<float>(g_fb.h)) : 1.0f;
            const float fovDegrees = std::clamp(cameraSettings.fovDegrees, 30.0f, 130.0f);
            const glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.1f, 1000.0f);
            const glm::vec3 camFwd = forwardDir(cam);
            const glm::mat4 view = glm::lookAt(cam.pos, cam.pos + camFwd, glm::vec3(0.0f, 1.0f, 0.0f));

            models.resize(n);
            renderPositions.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                const sim::Vec3 pSim = bs[i].prevPosition + (bs[i].position - bs[i].prevPosition) * alpha;
                const glm::vec3 pos(static_cast<float>(pSim.x), static_cast<float>(pSim.y), static_cast<float>(pSim.z));
                const auto r = static_cast<float>(bs[i].radius);
                renderPositions[i] = pos;

                glm::mat4 model(1.0f);
                model = glm::translate(model, pos);
                model = glm::scale(model, glm::vec3(r, r, r));
                models[i] = model;
            }

            glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(models.size() * sizeof(glm::mat4)),
                models.data(),
                GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glUseProgram(program);
            glUniformMatrix4fv(uView, 1, GL_FALSE, &view[0][0]);
            glUniformMatrix4fv(uProj, 1, GL_FALSE, &proj[0][0]);
            glUniform3f(uLightDir, 0.4f, 0.7f, 0.6f);
            glUniform3f(uBaseColor, 1.0f, 1.0f, 0.0f);
            glUniform1f(uAmbient, 0.25f);

            glBindVertexArray(sphereVao);
            glDrawElementsInstanced(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(n));
            glBindVertexArray(0);

            const DebugOverlay::PauseMenuHud pauseMenuHud = pauseMenu.buildHud(controls);

            DebugOverlay::TargetHud targetHud{};
            {
                const glm::vec3 rayOrigin = cam.pos;
                const glm::vec3 rayDir = glm::normalize(camFwd);

                int bestIdx = -1;
                float bestT = std::numeric_limits<float>::infinity();
                for (std::size_t i = 0; i < n; ++i) {
                    float t = 0.0f;
                    if (!raySphereHit(rayOrigin, rayDir, renderPositions[i], static_cast<float>(bs[i].radius), t)) {
                        continue;
                    }
                    if (t < bestT) {
                        bestT = t;
                        bestIdx = static_cast<int>(i);
                    }
                }

                if (bestIdx >= 0 && interfaceSettings.objectInfo) {
                    const auto i = static_cast<std::size_t>(bestIdx);
                    const glm::vec3 labelPos =
                        renderPositions[i] + glm::vec3(0.0f, static_cast<float>(bs[i].radius) * 1.6f, 0.0f);
                    float sx = 0.0f;
                    float sy = 0.0f;
                    if (worldToScreen(labelPos, view, proj, g_fb.w, g_fb.h, sx, sy)) {
                        targetHud.visible = true;
                        targetHud.xPx = sx;
                        targetHud.yPx = sy;
                        targetHud.lines.emplace_back("TARGET");

                        char line[64];
                        if (interfaceSettings.objectInfoMaterial) {
                            std::snprintf(line, sizeof(line), "E:%.2f", bs[i].restitution);
                            targetHud.lines.emplace_back(line);
                            std::snprintf(line, sizeof(line), "S:%.2f", bs[i].staticFriction);
                            targetHud.lines.emplace_back(line);
                            std::snprintf(line, sizeof(line), "D:%.2f", bs[i].dynamicFriction);
                            targetHud.lines.emplace_back(line);
                        }

                        if (interfaceSettings.objectInfoVelocity) {
                            std::snprintf(line, sizeof(line), "V:%.2f", bs[i].velocity.magnitude());
                            targetHud.lines.emplace_back(line);
                        }
                        if (interfaceSettings.objectInfoRadius) {
                            std::snprintf(line, sizeof(line), "R:%.2f", bs[i].radius);
                            targetHud.lines.emplace_back(line);
                        }
                        if (interfaceSettings.objectInfoMass) {
                            if (bs[i].invMass > 0.0) {
                                std::snprintf(line, sizeof(line), "M:%.2f", 1.0 / bs[i].invMass);
                            } else {
                                std::snprintf(line, sizeof(line), "M:INF");
                            }
                            targetHud.lines.emplace_back(line);
                        }
                    }
                }
            }

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            overlay.draw(
                g_fb.w,
                g_fb.h,
                simFrozen,
                simSpeed,
                hudFps,
                pauseMenuHud,
                targetHud,
                pauseMenu.uiScale(),
                interfaceSettings.showHud,
                noHudDebugLines);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            glfwSwapBuffers(window);
        }

        glDeleteBuffers(1, &instanceVbo);
        glDeleteBuffers(1, &sphereEbo);
        glDeleteBuffers(1, &sphereVbo);
        glDeleteVertexArrays(1, &sphereVao);
        glDeleteProgram(program);
        overlay.shutdown();

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
} // namespace app

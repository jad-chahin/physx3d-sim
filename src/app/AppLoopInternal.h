#ifndef PHYSICS3D_APPLOOPINTERNAL_H
#define PHYSICS3D_APPLOOPINTERNAL_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "input/Camera.h"
#include "sim/DefaultWorld.h"
#include "sim/World.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenuController.h"

namespace app_loop {

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
    struct SmoothedHudMetrics {
        sim::World::Metrics accumulated{};
        sim::World::Metrics displayed{};
        double elapsed = 0.0;
        std::size_t samples = 0;
    };

    struct PathTrail {
        std::vector<sim::Vec3> points{};
        std::size_t start = 0;
        std::size_t count = 0;
    };

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
    SmoothedHudMetrics hudMetrics{};
    std::vector<PathTrail> pathHistory{};
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
    ~GlfwSession();

    bool init();

private:
    bool active_ = false;
};

class WindowHandle {
public:
    WindowHandle() = default;
    WindowHandle(const WindowHandle&) = delete;
    WindowHandle& operator=(const WindowHandle&) = delete;
    ~WindowHandle();

    void reset(GLFWwindow* window);
    GLFWwindow* get() const;
    operator GLFWwindow*() const;

private:
    GLFWwindow* window_ = nullptr;
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

double consumeScrollDeltaY(AppLoopState& state);
int consumeLastPressedKey(AppLoopState& state);

void applySimulationSettings(sim::World& world, const ui::SimulationSettings& simSettings);
void updateFps(RuntimeState& runtime, double frameTime);
void syncPreviousState(sim::World& world);

void handleSimulationHotkeys(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::SimulationSettings& simSettings,
    bool pauseMenuOpen,
    RuntimeState& runtime,
    sim::World& world);

void updateCamera(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    double frameTime,
    input::Camera& cam);

void stepSimulation(sim::World& world, double frameTime, RuntimeState& runtime, bool pauseMenuOpen);
void reloadDefaultWorld(
    sim::World& world,
    RuntimeState& runtime,
    const ui::SimulationSettings& simSettings);

SceneView buildSceneView(
    const input::Camera& cam,
    const ui::CameraSettings& cameraSettings,
    const FramebufferSize& framebufferSize);

void buildRenderModels(
    const sim::World& world,
    double alpha,
    std::vector<glm::mat4>& models,
    std::vector<glm::vec3>& renderPositions);

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
    const std::vector<glm::mat4>& models);

OverlayRenderer::TargetHud buildTargetHud(
    const sim::World& world,
    const ui::InterfaceSettings& interfaceSettings,
    const input::Camera& cam,
    const SceneView& sceneView,
    const FramebufferSize& framebufferSize,
    const std::vector<glm::vec3>& renderPositions);

std::vector<std::string> buildHudDebugLines(
    const RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings);

void updatePathHistory(
    const sim::World& world,
    RuntimeState& runtime,
    const ui::InterfaceSettings& interfaceSettings);

std::vector<OverlayRenderer::ScreenLine> buildPathLines(
    const RuntimeState& runtime,
    const SceneView& sceneView,
    const FramebufferSize& framebufferSize,
    const ui::InterfaceSettings& interfaceSettings);

void drawOverlay(
    const OverlayRenderer& overlay,
    const FramebufferSize& framebufferSize,
    const RuntimeState& runtime,
    const OverlayRenderer::PauseMenuHud& pauseMenuHud,
    const OverlayRenderer::TargetHud& targetHud,
    const std::vector<OverlayRenderer::ScreenLine>& pathLines,
    float uiScale,
    const ui::InterfaceSettings& interfaceSettings,
    const std::vector<std::string>& hudDebugLines);

} // namespace app_loop

#endif // PHYSICS3D_APPLOOPINTERNAL_H

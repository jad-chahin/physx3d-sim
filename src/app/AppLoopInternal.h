#ifndef PHYSICS3D_APPLOOPINTERNAL_H
#define PHYSICS3D_APPLOOPINTERNAL_H

#include <cstddef>
#include <string>
#include <vector>

#include "app/ScenePresentation.h"
#include "app/SpatialHud.h"
#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/SceneRenderer.h"
#include "sim/DefaultWorld.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

#include <GLFW/glfw3.h>

namespace app_loop {

constexpr double kTickRate = 60.0;
constexpr double kFixedDt = 1.0 / kTickRate;
constexpr double kFpsUpdateInterval = 0.5;
constexpr double kMaxCameraFrameTime = 0.25;
constexpr int kInternalMaxPhysicsStepsPerFrame = 512;

struct AppLoopState {
    render_scene::FramebufferSize framebufferSize{};
    double scrollDeltaY = 0.0;
    int lastPressedKey = GLFW_KEY_UNKNOWN;
};

struct RuntimeState {
    struct InputRuntime {
        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
        bool firstMouse = true;
        bool mouseCaptured = true;
    };

    struct SimulationRuntime {
        bool freezeWasDown = false;
        bool speedDownWasDown = false;
        bool speedUpWasDown = false;
        bool speedResetWasDown = false;
        bool simFrozen = false;
        double simSpeed = 1.0;

        struct FixedStepState {
            double lastFrameTime = 0.0;
            double accumulator = 0.0;
            double alpha = 0.0;
        } fixedStep{};
    };

    struct FpsRuntime {
        double elapsed = 0.0;
        double displayed = 0.0;
        int frames = 0;
    };

    InputRuntime input{};
    SimulationRuntime simulation{};
    FpsRuntime fps{};
    std::vector<render_scene::PathTrail> pathHistory{};
};

class SimulationController {
public:
    explicit SimulationController(sim::World world);

    std::vector<sim::Body>& mutableBodies();
    [[nodiscard]] const std::vector<sim::Body>& bodies() const;
    [[nodiscard]] bool hasBodies() const;

    void applySettings(const ui::SimulationSettings& simSettings);
    void handleHotkeys(
        GLFWwindow* window,
        const input::ControlBindings& controls,
        const ui::SimulationSettings& simSettings,
        bool pauseMenuOpen,
        RuntimeState& runtime);
    void step(RuntimeState& runtime, bool pauseMenuOpen, double frameTime);
    void reset(RuntimeState& runtime, const ui::SimulationSettings& simSettings);

private:
    sim::World world_;

    void syncPreviousState_();
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

double consumeScrollDeltaY(AppLoopState& state);
int consumeLastPressedKey(AppLoopState& state);

void updateFps(RuntimeState& runtime, double frameTime);

void updateCamera(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    double frameTime,
    input::Camera& cam);

void buildSceneSnapshot(
    const SimulationController& simulation,
    const RuntimeState& runtime,
    double alpha,
    render_scene::SceneSnapshot& snapshot);

void drawOverlay(
    const OverlayRenderer& overlay,
    const render_scene::FramebufferSize& framebufferSize,
    const RuntimeState& runtime,
    const ui::MenuView& menuView,
    const OverlayRenderer::SpatialHud& spatialHud,
    const OverlayRenderer::TargetHud& targetHud,
    const std::vector<OverlayRenderer::ScreenLine>& pathLines,
    float uiScale,
    const ui::InterfaceSettings& interfaceSettings);

} // namespace app_loop

#endif // PHYSICS3D_APPLOOPINTERNAL_H


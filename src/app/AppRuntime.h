#ifndef PHYSICS3D_APP_RUNTIME_H
#define PHYSICS3D_APP_RUNTIME_H

#include <cstdint>
#include <vector>

#include "app/CameraFocus.h"
#include "render/SceneRenderer.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
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
        float scrollDeltaY = 0.0f;
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
        double elapsedTime = 0.0;
        std::uint64_t sceneRevision = 1;

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

    struct FocusRuntime {
        bool focusWasDown = false;
        CameraFocusState camera{};
    };

    InputRuntime input{};
    SimulationRuntime simulation{};
    FpsRuntime fps{};
    FocusRuntime focus{};
    std::uint64_t pathHistoryRevision = 0;
    std::vector<render_scene::PathTrail> pathHistory{};
};

} // namespace app_loop

#endif // PHYSICS3D_APP_RUNTIME_H

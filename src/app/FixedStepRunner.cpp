#include "app/AppLoopInternal.h"

#include <algorithm>
#include <cmath>

namespace app_loop {
namespace {
    constexpr double kHudMetricsUpdateInterval = 0.5;
    constexpr std::size_t kHudMetricsSampleEverySteps = 4;

    void resetFixedStepState(RuntimeState& runtime) {
        runtime.simulation.fixedStep.lastFrameTime = glfwGetTime();
        runtime.simulation.fixedStep.accumulator = 0.0;
        runtime.simulation.fixedStep.alpha = 0.0;
    }

    void accumulateHudMetrics(RuntimeState& runtime, const sim::World::Metrics& metrics) {
        auto& accum = runtime.hudMetrics.accumulated;
        accum.gravityBodies += metrics.gravityBodies;
        accum.gravityPairs += metrics.gravityPairs;
        accum.broadphaseCandidatesDiscrete += metrics.broadphaseCandidatesDiscrete;
        accum.broadphaseCandidatesSwept += metrics.broadphaseCandidatesSwept;
        accum.pairsVisited += metrics.pairsVisited;
        accum.pairsImpulseApplied += metrics.pairsImpulseApplied;
        accum.ccdEvents += metrics.ccdEvents;
        accum.ccdZeroTimePairsResolved += metrics.ccdZeroTimePairsResolved;
        accum.ccdIterations += metrics.ccdIterations;
        accum.ccdBudgetExhaustions += metrics.ccdBudgetExhaustions;
        accum.warmStartedPairs += metrics.warmStartedPairs;
        accum.manifoldActivePairs += metrics.manifoldActivePairs;
        accum.sanitizedBodies += metrics.sanitizedBodies;
        accum.sanitizedFields += metrics.sanitizedFields;
        ++runtime.hudMetrics.samples;
    }

    void maybeSampleHudMetrics(RuntimeState& runtime, const sim::World::Metrics& metrics) {
        runtime.hudMetrics.elapsed += kFixedDt;
        ++runtime.hudMetrics.stepsSinceSample;
        if (runtime.hudMetrics.stepsSinceSample < kHudMetricsSampleEverySteps) {
            return;
        }

        runtime.hudMetrics.stepsSinceSample = 0;
        accumulateHudMetrics(runtime, metrics);
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
        displayed.gravityBodies = averageMetric(accum.gravityBodies, samples);
        displayed.gravityPairs = averageMetric(accum.gravityPairs, samples);
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
} // namespace

void updateFps(RuntimeState& runtime, const double frameTime) {
    runtime.fps.elapsed += frameTime;
    ++runtime.fps.frames;
    if (runtime.fps.elapsed >= kFpsUpdateInterval) {
        runtime.fps.displayed = static_cast<double>(runtime.fps.frames) / runtime.fps.elapsed;
        runtime.fps.elapsed = 0.0;
        runtime.fps.frames = 0;
    }
}

void SimulationController::handleHotkeys(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::SimulationSettings& simSettings,
    const bool pauseMenuOpen,
    RuntimeState& runtime)
{
    const bool freezeDown = input::isBindingPressed(window, controls.freeze);
    if (!pauseMenuOpen && freezeDown && !runtime.simulation.freezeWasDown) {
        runtime.simulation.simFrozen = !runtime.simulation.simFrozen;
        resetFixedStepState(runtime);
        syncPreviousState_();
    }
    runtime.simulation.freezeWasDown = freezeDown;

    const bool speedDown = input::isBindingPressed(window, controls.speedDown);
    if (!pauseMenuOpen && speedDown && !runtime.simulation.speedDownWasDown) {
        runtime.simulation.simSpeed = std::max(simSettings.minSimSpeed, runtime.simulation.simSpeed * 0.5);
    }
    runtime.simulation.speedDownWasDown = speedDown;

    const bool speedUp = input::isBindingPressed(window, controls.speedUp);
    if (!pauseMenuOpen && speedUp && !runtime.simulation.speedUpWasDown) {
        runtime.simulation.simSpeed = std::min(simSettings.maxSimSpeed, runtime.simulation.simSpeed * 2.0);
    }
    runtime.simulation.speedUpWasDown = speedUp;

    const bool speedReset = input::isBindingPressed(window, controls.speedReset);
    if (!pauseMenuOpen && speedReset && !runtime.simulation.speedResetWasDown) {
        runtime.simulation.simSpeed = std::clamp(1.0, simSettings.minSimSpeed, simSettings.maxSimSpeed);
    }
    runtime.simulation.speedResetWasDown = speedReset;
}

void updateCamera(
    GLFWwindow* window,
    const input::ControlBindings& controls,
    const ui::CameraSettings& cameraSettings,
    RuntimeState& runtime,
    const double frameTime,
    input::Camera& cam)
{
    if (!runtime.input.mouseCaptured) {
        return;
    }

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    if (runtime.input.firstMouse) {
        runtime.input.lastMouseX = mx;
        runtime.input.lastMouseY = my;
        runtime.input.firstMouse = false;
    }

    const double dx = mx - runtime.input.lastMouseX;
    const double dy = my - runtime.input.lastMouseY;
    runtime.input.lastMouseX = mx;
    runtime.input.lastMouseY = my;

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

void SimulationController::step(RuntimeState& runtime, const bool pauseMenuOpen, const double frameTime) {
    auto& fixedStep = runtime.simulation.fixedStep;
    if (runtime.simulation.simFrozen || pauseMenuOpen) {
        fixedStep.accumulator = 0.0;
        fixedStep.alpha = 0.0;
        return;
    }

    if (runtime.simulation.simSpeed <= 0.0) {
        fixedStep.accumulator = 0.0;
        fixedStep.alpha = 0.0;
        return;
    }

    const double scaledFrameTime = std::max(0.0, frameTime * runtime.simulation.simSpeed);
    if (!std::isfinite(scaledFrameTime)) {
        fixedStep.accumulator = 0.0;
        fixedStep.alpha = 0.0;
        return;
    }

    fixedStep.accumulator += scaledFrameTime;

    int steps = 0;
    while (fixedStep.accumulator >= kFixedDt && steps < kInternalMaxPhysicsStepsPerFrame) {
        syncPreviousState_();
        world_.beginDiagnostics();
        world_.step(kFixedDt);
        world_.finalizeDiagnostics();
        maybeSampleHudMetrics(runtime, world_.metrics());
        fixedStep.accumulator -= kFixedDt;
        ++steps;
    }

    fixedStep.alpha = std::clamp(fixedStep.accumulator / kFixedDt, 0.0, 1.0);
    refreshDisplayedHudMetrics(runtime);
}

} // namespace app_loop


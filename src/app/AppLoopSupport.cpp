#include "app/AppLoopInternal.h"

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

void reloadDefaultWorld(
    sim::World& world,
    RuntimeState& runtime,
    const ui::SimulationSettings& simSettings)
{
    world = sim::makeDefaultWorld();
    applySimulationSettings(world, simSettings);
    runtime.simulation.fixedStep.lastFrameTime = glfwGetTime();
    runtime.simulation.fixedStep.accumulator = 0.0;
    runtime.simulation.fixedStep.alpha = 0.0;
    runtime.fps = {};
    runtime.simulation.freezeWasDown = false;
    runtime.simulation.speedDownWasDown = false;
    runtime.simulation.speedUpWasDown = false;
    runtime.simulation.speedResetWasDown = false;
    runtime.hudMetrics = {};
    runtime.pathHistory.clear();
    syncPreviousState(world);
}

} // namespace app_loop


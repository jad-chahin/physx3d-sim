#include "app/FrameUpdate.h"
#include "app/SimulationController.h"
#include "app/Windowing.h"

#include "sim/DefaultWorld.h"

namespace app_loop {

SimulationController::SimulationController(sim::World world)
    : world_(std::move(world)) {}

std::vector<sim::Body>& SimulationController::mutableBodies() {
    return world_.bodies();
}

const std::vector<sim::Body>& SimulationController::bodies() const {
    return world_.bodies();
}

bool SimulationController::hasBodies() const {
    return !world_.bodies().empty();
}

void SimulationController::applySettings(const ui::SimulationSettings& simSettings) {
    world_.params().enableGravity = simSettings.gravityEnabled;
    world_.params().G = simSettings.gravityStrength;
    world_.params().enableCollisions = simSettings.collisionsEnabled;
    world_.params().velocityIterations = simSettings.velocityIterations;
    world_.params().positionIterations = simSettings.positionIterations;
    world_.params().restitution = simSettings.globalRestitution;
}

void SimulationController::syncPreviousState_() {
    for (auto& body : world_.bodies()) {
        body.prevPosition = body.position;
        body.prevOrientation = body.orientation;
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

bool movementCanExitCameraFocus(
    const bool pauseMenuOpen,
    const bool focusStartedThisFrame)
{
    return !pauseMenuOpen && !focusStartedThisFrame;
}

void SimulationController::reset(
    RuntimeState& runtime,
    const ui::SimulationSettings& simSettings)
{
    world_ = sim::makeDefaultWorld();
    applySettings(simSettings);
    runtime.simulation.fixedStep.lastFrameTime = glfwGetTime();
    runtime.simulation.fixedStep.accumulator = 0.0;
    runtime.simulation.fixedStep.alpha = 0.0;
    runtime.fps = {};
    runtime.simulation.freezeWasDown = false;
    runtime.simulation.speedDownWasDown = false;
    runtime.simulation.speedUpWasDown = false;
    runtime.simulation.speedResetWasDown = false;
    runtime.focus = {};
    runtime.simulation.elapsedTime = 0.0;
    ++runtime.simulation.sceneRevision;
    ++runtime.pathHistoryRevision;
    runtime.pathHistory.clear();
    syncPreviousState_();
}

} // namespace app_loop


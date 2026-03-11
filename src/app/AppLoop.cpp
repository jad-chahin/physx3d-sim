#include "app/AppLoop.h"
#include "app/AppLoopInternal.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "input/Camera.h"
#include "render/ShaderUtil.h"
#include "render/SphereMesh.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenuController.h"

namespace {

void registerCallbacks(GLFWwindow* window, app_loop::AppLoopState& appState) {
    glfwSetWindowUserPointer(window, &appState);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* callbackWindow, const int w, const int h) {
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr) {
            state->framebufferSize.w = w;
            state->framebufferSize.h = h;
        }
    });

    glfwSetKeyCallback(window, [](GLFWwindow* callbackWindow, const int key, const int scancode, const int action, const int mods) {
        (void)scancode;
        (void)mods;
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr && action == GLFW_PRESS && key != GLFW_KEY_UNKNOWN) {
            state->lastPressedKey = key;
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* callbackWindow, const double xoffset, const double yoffset) {
        (void)xoffset;
        auto* state = static_cast<app_loop::AppLoopState*>(glfwGetWindowUserPointer(callbackWindow));
        if (state != nullptr) {
            state->scrollDeltaY += yoffset;
        }
    });
}

void configureWindow(GLFWwindow* window, app_loop::AppLoopState& appState) {
    constexpr int kMinWindowWidth = 960;
    constexpr int kMinWindowHeight = 540;

    glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwGetFramebufferSize(window, &appState.framebufferSize.w, &appState.framebufferSize.h);
    registerCallbacks(window, appState);
}

bool initGlad() {
    return gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
}

void configureOpenGl() {
    int flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugCallback, nullptr);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

struct RenderResources {
    GLuint program = 0;
    GLint uView = -1;
    GLint uProj = -1;
    GLint uLightDir = -1;
    GLint uBaseColor = -1;
    GLint uAmbient = -1;
    GLuint sphereVao = 0;
    GLuint sphereVbo = 0;
    GLuint sphereEbo = 0;
    GLuint instanceVbo = 0;
    GLsizei sphereIndexCount = 0;

    ~RenderResources() {
        if (instanceVbo) glDeleteBuffers(1, &instanceVbo);
        if (sphereEbo) glDeleteBuffers(1, &sphereEbo);
        if (sphereVbo) glDeleteBuffers(1, &sphereVbo);
        if (sphereVao) glDeleteVertexArrays(1, &sphereVao);
        if (program) glDeleteProgram(program);
    }
};

bool initRenderResources(RenderResources& resources) {
    std::vector<float> sphereVerts;
    std::vector<std::uint32_t> sphereIdx;
    buildSphereMesh(16, 32, sphereVerts, sphereIdx);
    resources.sphereIndexCount = static_cast<GLsizei>(sphereIdx.size());

    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    if (vs == 0 || fs == 0) {
        std::cerr << "Shader compile failed; exiting.\n";
        return false;
    }

    resources.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (resources.program == 0) {
        std::cerr << "Program link failed; exiting.\n";
        return false;
    }

    resources.uView = glGetUniformLocation(resources.program, "uView");
    resources.uProj = glGetUniformLocation(resources.program, "uProj");
    resources.uLightDir = glGetUniformLocation(resources.program, "uLightDir");
    resources.uBaseColor = glGetUniformLocation(resources.program, "uBaseColor");
    resources.uAmbient = glGetUniformLocation(resources.program, "uAmbient");
    if (resources.uView < 0 || resources.uProj < 0 || resources.uLightDir < 0 ||
        resources.uBaseColor < 0 || resources.uAmbient < 0)
    {
        std::cerr << "Missing uniform(s). Check shader names match exactly.\n";
    }

    glGenVertexArrays(1, &resources.sphereVao);
    glGenBuffers(1, &resources.sphereVbo);
    glGenBuffers(1, &resources.sphereEbo);
    glGenBuffers(1, &resources.instanceVbo);

    glBindVertexArray(resources.sphereVao);

    glBindBuffer(GL_ARRAY_BUFFER, resources.sphereVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereVerts.size() * sizeof(float)),
        sphereVerts.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, resources.sphereEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereIdx.size() * sizeof(std::uint32_t)),
        sphereIdx.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, resources.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    constexpr std::size_t vec4Size = 4 * sizeof(float);
    constexpr std::size_t mat4Size = 4 * vec4Size;
    for (GLuint attrib = 0; attrib < 4; ++attrib) {
        glEnableVertexAttribArray(2 + attrib);
        glVertexAttribPointer(
            2 + attrib,
            4,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(mat4Size),
            reinterpret_cast<void*>(attrib * vec4Size));
        glVertexAttribDivisor(2 + attrib, 1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return true;
}

void printGlInfo() {
    std::cout << "Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION) << "\n";
}

void updatePauseMenu(
    GLFWwindow* window,
    app_loop::AppLoopState& appState,
    app_loop::RuntimeState& runtime,
    input::ControlBindings& controls,
    ui::PauseMenuController& pauseMenu,
    sim::World& world,
    const std::string& controlsConfigPath)
{
    const int pressedKey = app_loop::consumeLastPressedKey(appState);
    const auto scrollDeltaY = static_cast<float>(app_loop::consumeScrollDeltaY(appState));

    pauseMenu.updateEscapeState(
        window,
        runtime.mouseCaptured,
        runtime.firstMouse,
        runtime.lastTime,
        runtime.accumulator,
        world.bodies());
    pauseMenu.handlePointerInput(window, controls, controlsConfigPath, scrollDeltaY);
    pauseMenu.handlePressedKey(window, pressedKey, controls, controlsConfigPath);
    pauseMenu.updateContinuousInput(window, controls, controlsConfigPath);
}

void updateFrameState(
    GLFWwindow* window,
    sim::World& world,
    const input::ControlBindings& controls,
    const ui::PauseMenuController& pauseMenu,
    app_loop::RuntimeState& runtime,
    input::Camera& cam)
{
    const auto& simSettings = pauseMenu.simulationSettings();
    const auto& cameraSettings = pauseMenu.cameraSettings();

    app_loop::applySimulationSettings(world, simSettings);
    runtime.simSpeed = std::clamp(runtime.simSpeed, simSettings.minSimSpeed, simSettings.maxSimSpeed);
    app_loop::handleSimulationHotkeys(window, controls, simSettings, pauseMenu.isOpen(), runtime, world);

    const double now = glfwGetTime();
    double frameTime = std::min(now - runtime.lastTime, 0.25);
    runtime.lastTime = now;

    app_loop::updateFps(runtime, frameTime);
    app_loop::updateCamera(window, controls, cameraSettings, runtime, frameTime, cam);
    app_loop::stepSimulation(world, frameTime, runtime, pauseMenu.isOpen());
}

void clearFrame(const app_loop::RuntimeState& runtime, const ui::PauseMenuController& pauseMenu) {
    if (runtime.simFrozen && !pauseMenu.isOpen()) {
        glClearColor(0.20f, 0.02f, 0.02f, 1.0f);
    } else {
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

} // namespace

namespace app {

int runApp(sim::World world) {
    app_loop::GlfwSession glfwSession;
    if (!glfwSession.init()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    app_loop::WindowHandle window;
    window.reset(glfwCreateWindow(1920, 1080, "physics3d", nullptr, nullptr));
    if (window.get() == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        return 1;
    }

    app_loop::AppLoopState appState{};
    configureWindow(window, appState);

    input::Camera cam{};
    app_loop::RuntimeState runtime{};
    input::ControlBindings controls{};
    const std::string controlsConfigPath = "controls.cfg";
    input::loadControlBindings(controls, controlsConfigPath);

    ui::PauseMenuController pauseMenu;
    pauseMenu.loadSettings("settings.cfg");
    pauseMenu.applyCurrentDisplaySettings(window);

    if (!initGlad()) {
        std::cerr << "Failed to initialize GLAD\n";
        return 1;
    }

    glViewport(0, 0, appState.framebufferSize.w, appState.framebufferSize.h);
    printGlInfo();
    configureOpenGl();

    RenderResources renderResources;
    if (!initRenderResources(renderResources)) {
        return 1;
    }

    OverlayRenderer overlay;
    overlay.init();
    runtime.lastTime = glfwGetTime();

    std::vector<glm::mat4> models;
    std::vector<glm::vec3> renderPositions;
    app_loop::InstanceBufferState instanceBufferState{};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glViewport(0, 0, appState.framebufferSize.w, appState.framebufferSize.h);

        updatePauseMenu(window, appState, runtime, controls, pauseMenu, world, controlsConfigPath);
        updateFrameState(window, world, controls, pauseMenu, runtime, cam);
        clearFrame(runtime, pauseMenu);

        const OverlayRenderer::PauseMenuHud pauseMenuHud = pauseMenu.buildHud(controls);
        OverlayRenderer::TargetHud targetHud{};
        if (!world.bodies().empty()) {
            const app_loop::SceneView sceneView =
                app_loop::buildSceneView(cam, pauseMenu.cameraSettings(), appState.framebufferSize);
            const double alpha = std::clamp(runtime.accumulator / app_loop::kFixedDt, 0.0, 1.0);
            app_loop::buildRenderModels(world, alpha, models, renderPositions);
            app_loop::drawBodies(
                renderResources.instanceVbo,
                renderResources.program,
                renderResources.uView,
                renderResources.uProj,
                renderResources.uLightDir,
                renderResources.uBaseColor,
                renderResources.uAmbient,
                renderResources.sphereVao,
                renderResources.sphereIndexCount,
                sceneView,
                instanceBufferState,
                models);
            targetHud = app_loop::buildTargetHud(
                world,
                pauseMenu.interfaceSettings(),
                cam,
                sceneView,
                appState.framebufferSize,
                renderPositions);
        }

        app_loop::drawOverlay(
            overlay,
            appState.framebufferSize,
            runtime,
            pauseMenuHud,
            targetHud,
            pauseMenu.uiScale(),
            pauseMenu.interfaceSettings());

        glfwSwapBuffers(window);
    }

    overlay.shutdown();
    return 0;
}

} // namespace app

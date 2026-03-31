#include "app/AppBootstrap.h"

#include <iostream>

#include <glad/glad.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "render/ShaderUtil.h"

namespace {

void registerCallbacks(GLFWwindow* window, app_loop::AppLoopState& appState)
{
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

} // namespace

namespace app_loop {

void configureWindow(GLFWwindow* window, AppLoopState& appState)
{
    constexpr int kMinWindowWidth = 960;
    constexpr int kMinWindowHeight = 540;

    glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwGetFramebufferSize(window, &appState.framebufferSize.w, &appState.framebufferSize.h);
    registerCallbacks(window, appState);
}

bool initGlad()
{
    return gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
}

void configureOpenGl()
{
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

void printGlInfo()
{
    std::cout << "Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION) << "\n";
}

} // namespace app_loop

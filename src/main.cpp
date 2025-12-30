#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include "sim/World.h"


static void APIENTRY glDebugCallback(
    const GLenum source, const GLenum type, const GLuint id, const GLenum severity,
    const GLsizei length, const GLchar* message, const void* userParam)
{
    (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
    std::cerr << "[GL DEBUG] " << message << "\n";
}


int main() {
    // Init world
    sim::Body a{};
    a.invMass = 1.0;

    sim::Body b{};
    b.position = sim::Vec3(10.0, 0.0, 0.0);
    b.velocity = sim::Vec3(-1.0, 0.0, 0.0);
    b.invMass  = 0.5;
    b.radius   = 2.0;

    std::vector<sim::Body> bodies;
    bodies.push_back(a);
    bodies.push_back(b);

    sim::World world(std::move(bodies)); // default params


    // Rendering
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    // Request an OpenGL 3.3 core profile context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "physics3d", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    // VSync 0=OFF 1=ON
    glfwSwapInterval(1);

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

    // Set initial viewport + update it on resize
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, const int w, const int h) {
        glViewport(0, 0, w, h);
    });

    glEnable(GL_DEPTH_TEST);

    constexpr double tickRate = 60.0;
    constexpr double dt = 1.0 / tickRate;

    double lastTime = glfwGetTime();
    double accumulator = 0.0;

    while (!glfwWindowShouldClose(window)) {
        constexpr int maxStepsPerFrame = 5;
        glfwPollEvents();

        const double now = glfwGetTime();
        double frameTime = now - lastTime;
        lastTime = now;

        frameTime = std::min(frameTime, maxStepsPerFrame * dt);

        accumulator += frameTime;

        while (accumulator >= dt) {
            world.step(dt);
            accumulator -= dt;
        }

        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

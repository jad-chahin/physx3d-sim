#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>
#include <utility> // std::move

#include "render/ShaderUtil.h"
#include "render/SphereMesh.h"
#include "input/Camera.h"
#include "ui/DebugOverlay.h"
#include "sim/World.h"


struct FramebufferSize {
    int w = 0;
    int h = 0;
};
static FramebufferSize g_fb;


int main() {
    // Init world
    sim::Body a{};
    a.invMass = 1.0;
    a.prevPosition = a.position;

    sim::Body b{};
    b.position = sim::Vec3(10.0, -10.0, 10.0);
    b.velocity = sim::Vec3(-2.0, 2.0, -2.0);
    b.invMass  = 0.5;
    b.radius   = 2.0;
    b.prevPosition = b.position;


    sim::Body c{};
    c.position = sim::Vec3(20.0, -20.0, 10.0);
    c.velocity = sim::Vec3(0.0, 0.0, 0.0);
    c.invMass  = 0.5;
    c.radius   = 2.0;
    c.prevPosition = c.position;

    std::vector<sim::Body> bodies;
    bodies.push_back(a);
    bodies.push_back(b);
    bodies.push_back(c);

    sim::World world(std::move(bodies)); // Default params

    // Rendering
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    // 3.3 core profile context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "physics3d", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;

    Camera cam{};

    bool mouseCaptured = true;
    bool escWasDown = false;
    bool spaceWasDown = false;
    bool paused = false;

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

    // Framebuffer size tracking + viewport
    glfwGetFramebufferSize(window, &g_fb.w, &g_fb.h);
    glViewport(0, 0, g_fb.w, g_fb.h);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, const int w, const int h) {
        g_fb.w = w;
        g_fb.h = h;
        glViewport(0, 0, w, h);
    });

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Build sphere mesh (unit sphere)
    std::vector<float> sphereVerts;
    std::vector<std::uint32_t> sphereIdx;
    buildSphereMesh(/*stacks=*/16, /*slices=*/32, sphereVerts, sphereIdx);
    const auto sphereIndexCount = static_cast<GLsizei>(sphereIdx.size());

    // Compile/link shaders
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

    // Uniform locations
    const GLint uView     = glGetUniformLocation(program, "uView");     // NOLINT
    const GLint uProj     = glGetUniformLocation(program, "uProj");     // NOLINT
    const GLint uLightDir = glGetUniformLocation(program, "uLightDir"); // NOLINT
    const GLint uBaseColor= glGetUniformLocation(program, "uBaseColor");// NOLINT
    const GLint uAmbient  = glGetUniformLocation(program, "uAmbient");  // NOLINT

    if (uView < 0 || uProj < 0 || uLightDir < 0 || uBaseColor < 0 || uAmbient < 0) {
        std::cerr << "Missing uniform(s). Check shader names match exactly.\n";
    }

    // Sphere VAO/VBO/EBO + instance VBO
    GLuint sphereVao = 0, sphereVbo = 0, sphereEbo = 0, instanceVbo = 0;
    glGenVertexArrays(1, &sphereVao);
    glGenBuffers(1, &sphereVbo);
    glGenBuffers(1, &sphereEbo);
    glGenBuffers(1, &instanceVbo);

    glBindVertexArray(sphereVao);

    // Vertex buffer: pos+normal (6 floats)
    glBindBuffer(GL_ARRAY_BUFFER, sphereVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereVerts.size() * sizeof(float)),
        sphereVerts.data(),
        GL_STATIC_DRAW
    );

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereIdx.size() * sizeof(std::uint32_t)),
        sphereIdx.data(),
        GL_STATIC_DRAW
    );

    // attrib 0: position
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(0)
    );
    glEnableVertexAttribArray(0);

    // attrib 1: normal
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    // Instance buffer: mat4 model (16 floats) per instance
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    constexpr std::size_t vec4Size = 4 * sizeof(float);
    constexpr std::size_t mat4Size = 4 * vec4Size;

    // locations 2..5 are the 4 columns of the model matrix
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

    // Tick loop
    constexpr double tickRate = 60.0;
    constexpr double dt = 1.0 / tickRate;

    double lastTime = glfwGetTime();
    double accumulator = 0.0;

    std::vector<glm::mat4> models;

    while (!glfwWindowShouldClose(window)) {
        constexpr int maxStepsPerFrame = 10;
        glfwPollEvents();
        auto& bs = world.bodies();

        // ESC toggles mouse capture
        const bool escDown = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
        if (escDown && !escWasDown) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            firstMouse = true; // prevent jump when recapturing
        }
        escWasDown = escDown;

        // SPACE toggles pause
        const bool spaceDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (spaceDown && !spaceWasDown) {
            paused = !paused;

            // prevent a big frameTime after pausing/unpausing
            lastTime = glfwGetTime();
            accumulator = 0.0;

            // freeze interpolation cleanly
            for (auto& body : bs) {
                body.prevPosition = body.position;
            }
        }
        spaceWasDown = spaceDown;


        const double now = glfwGetTime();
        double frameTime = now - lastTime;
        lastTime = now;

        frameTime = std::min(frameTime, maxStepsPerFrame * dt);

        // Mouse look
        if (mouseCaptured) {
            double mx = 0.0, my = 0.0;
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

            cam.yaw   += static_cast<float>(dx) * cam.lookSpeed;
            cam.pitch -= static_cast<float>(dy) * cam.lookSpeed;
            clampPitch(cam);

            // Keyboard move (uses real frameTime)
            const float move = cam.moveSpeed * static_cast<float>(frameTime);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.pos += forwardDir(cam) * move;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.pos -= forwardDir(cam) * move;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.pos += rightDir(cam)   * move;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.pos -= rightDir(cam)   * move;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cam.pos += glm::vec3(0,1,0) * move;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cam.pos -= glm::vec3(0,1,0) * move;
        }

        if (!paused) {
            accumulator += frameTime;

            while (accumulator >= dt) {
                for (auto& body : bs) {
                    body.prevPosition = body.position;
                }
                world.step(dt);
                accumulator -= dt;
            }
        }

        // Interpolation factor for rendering
        const double alpha = std::clamp(accumulator / dt, 0.0, 1.0);

        const std::size_t n = bs.size();

        if (paused) {
            glClearColor(0.20f, 0.02f, 0.02f, 1.0f); // paused tint
        } else {
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (n == 0) {
            glfwSwapBuffers(window);
            continue;
        }

        // Camera + projection
        const float aspect = (g_fb.h > 0) ? (static_cast<float>(g_fb.w) / static_cast<float>(g_fb.h)) : 1.0f;

        const glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
        const glm::vec3 camFwd = forwardDir(cam);
        const glm::mat4 view = glm::lookAt(cam.pos, cam.pos + camFwd, glm::vec3(0.0f, 1.0f, 0.0f));


        // Build instance model matrices from interpolated positions
        models.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const sim::Vec3 pSim = bs[i].prevPosition + (bs[i].position - bs[i].prevPosition) * alpha;

            const glm::vec3 pos(static_cast<float>(pSim.x), static_cast<float>(pSim.y), static_cast<float>(pSim.z));
            const auto r = static_cast<float>(bs[i].radius);

            glm::mat4 model(1.0f);
            model = glm::translate(model, pos);
            model = glm::scale(model, glm::vec3(r, r, r));

            models[i] = model;
        }

        // Upload instance buffer
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(models.size() * sizeof(glm::mat4)),
            models.data(),
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Draw
        glUseProgram(program);

        glUniformMatrix4fv(uView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(uProj, 1, GL_FALSE, &proj[0][0]);

        // Light coming from above-right-front (towards the scene)
        glUniform3f(uLightDir, 0.4f, 0.7f, 0.6f);
        glUniform3f(uBaseColor, 1.0f, 1.0f, 0.0f);
        glUniform1f(uAmbient, 0.25f);

        glBindVertexArray(sphereVao);
        glDrawElementsInstanced(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(n));
        glBindVertexArray(0);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        overlay.draw(g_fb.w, g_fb.h, paused);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
    }

    // Cleanup
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

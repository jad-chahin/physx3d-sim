#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cmath>
#include <iostream>
#include <limits>
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

static bool raySphereHit(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& center,
    const float radius,
    float& outT)
{
    const glm::vec3 oc = rayOrigin - center;
    const float b = glm::dot(oc, rayDir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }
    const float s = std::sqrt(disc);
    const float t0 = -b - s;
    const float t1 = -b + s;
    if (t0 > 0.0f) {
        outT = t0;
        return true;
    }
    if (t1 > 0.0f) {
        outT = t1;
        return true;
    }
    return false;
}

static bool worldToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    const int fbw,
    const int fbh,
    float& outXPx,
    float& outYPx)
{
    const glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) {
        return false;
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
        return false;
    }

    outXPx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(fbw);
    outYPx = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(fbh);
    return true;
}


int main() {
    // Init world
    std::vector<sim::Body> bodies;
    bodies.reserve(160);
    std::vector<sim::World::DistanceJoint> joints;
    joints.reserve(192);

    auto pushBall = [&bodies](
        const sim::Vec3& position,
        const sim::Vec3& velocity,
        const double invMass,
        const double radius,
        const double restitution,
        const double staticFriction,
        const double dynamicFriction)
        -> std::size_t
    {
        sim::Body b{};
        b.position = position;
        b.prevPosition = position;
        b.velocity = velocity;
        b.invMass = std::max(0.0, invMass);
        b.radius = std::max(0.05, radius);
        b.restitution = std::clamp(restitution, 0.0, 1.0);
        b.staticFriction = std::max(0.0, staticFriction);
        b.dynamicFriction = std::max(0.0, std::min(dynamicFriction, b.staticFriction));
        bodies.push_back(b);
        return bodies.size() - 1;
    };

    auto addJoint = [&joints](
        const std::size_t a,
        const std::size_t b,
        const double rest,
        const double stiffness,
        const double damping,
        const bool collideConnected = false)
    {
        sim::World::DistanceJoint joint{};
        joint.bodyA = a;
        joint.bodyB = b;
        joint.restLength = rest;
        joint.stiffness = stiffness;
        joint.damping = damping;
        joint.collideConnected = collideConnected;
        joints.push_back(joint);
    };

    std::vector<std::size_t> chainA;
    chainA.reserve(12);
    constexpr double chainRadius = 0.38;
    constexpr double chainSpacing = chainRadius * 2.0;
    chainA.push_back(pushBall(sim::Vec3(-15.0, 16.0, -2.0), sim::Vec3(0.0, 0.0, 0.0), 0.0, chainRadius, 0.25, 0.85, 0.70));
    for (int i = 1; i < 12; ++i) {
        const double z = (i % 2 == 0) ? 0.18 : -0.18;
        const std::size_t idx = pushBall(
            sim::Vec3(-15.0, 16.0 - static_cast<double>(i) * chainSpacing, z),
            sim::Vec3(0.0, 0.0, 0.0),
            1.0, chainRadius, 0.55, 0.40, 0.25);
        chainA.push_back(idx);
        bodies[idx].angularVelocity = sim::Vec3(0.0, 2.5 + 0.15 * static_cast<double>(i), 0.0);
    }
    bodies[chainA.back()].velocity = sim::Vec3(15.0, 0.0, 3.0);
    for (std::size_t i = 1; i < chainA.size(); ++i) {
        addJoint(chainA[i - 1], chainA[i], chainSpacing, 0.88, 0.12);
    }

    std::vector<std::size_t> chainB;
    chainB.reserve(9);
    chainB.push_back(pushBall(sim::Vec3(15.0, 15.0, 3.0), sim::Vec3(0.0, 0.0, 0.0), 0.0, chainRadius, 0.20, 0.90, 0.75));
    for (int i = 1; i < 9; ++i) {
        const std::size_t idx = pushBall(
            sim::Vec3(15.0 - 0.25 * static_cast<double>(i), 15.0 - static_cast<double>(i) * chainSpacing, 3.0),
            sim::Vec3(0.0, 0.0, 0.0),
            1.0, chainRadius, 0.60, 0.35, 0.20);
        chainB.push_back(idx);
        bodies[idx].angularVelocity = sim::Vec3(1.2, 0.8, 0.4);
    }
    bodies[chainB.back()].velocity = sim::Vec3(-18.0, 0.0, -8.0);
    for (std::size_t i = 1; i < chainB.size(); ++i) {
        addJoint(chainB[i - 1], chainB[i], chainSpacing, 0.86, 0.10);
    }

    std::vector<std::size_t> bridge;
    bridge.reserve(14);
    constexpr double bridgeR = 0.34;
    constexpr int bridgeLinks = 12;
    const std::size_t bridgeLeft = pushBall(sim::Vec3(-8.0, 8.5, -9.0), sim::Vec3(0.0, 0.0, 0.0), 0.0, bridgeR, 0.20, 0.95, 0.75);
    const std::size_t bridgeRight = pushBall(sim::Vec3(8.0, 8.5, -9.0), sim::Vec3(0.0, 0.0, 0.0), 0.0, bridgeR, 0.20, 0.95, 0.75);
    bridge.push_back(bridgeLeft);
    for (int i = 1; i <= bridgeLinks; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(bridgeLinks + 1);
        const double x = -8.0 + 16.0 * t;
        const double sag = 2.2 * (1.0 - std::pow(2.0 * t - 1.0, 2.0));
        bridge.push_back(pushBall(sim::Vec3(x, 8.5 - sag, -9.0), sim::Vec3(0.0, 0.0, 0.0), 0.8, bridgeR, 0.45, 0.55, 0.30));
    }
    bridge.push_back(bridgeRight);
    for (std::size_t i = 1; i < bridge.size(); ++i) {
        addJoint(bridge[i - 1], bridge[i], bridgeR * 2.0, 0.83, 0.11);
    }

    const std::size_t heavyA = pushBall(sim::Vec3(0.0, 3.0, 0.0), sim::Vec3(0.0, 0.0, 0.0), 0.015, 3.0, 0.25, 0.90, 0.75);
    const std::size_t heavyB = pushBall(sim::Vec3(0.0, -8.0, 5.5), sim::Vec3(0.0, 0.0, 0.0), 0.008, 3.6, 0.20, 0.95, 0.80);
    bodies[heavyA].angularVelocity = sim::Vec3(0.0, 1.2, 0.0);
    bodies[heavyB].angularVelocity = sim::Vec3(0.0, -0.8, 0.4);
    pushBall(sim::Vec3(-4.0, 1.0, 6.0), sim::Vec3(10.0, 1.0, -7.0), 0.45, 1.5, 0.65, 0.35, 0.20);
    pushBall(sim::Vec3(4.0, 2.5, -2.0), sim::Vec3(-9.0, -1.0, 5.5), 0.55, 1.35, 0.60, 0.30, 0.18);

    const std::size_t center = pushBall(sim::Vec3(0.0, 6.5, 10.0), sim::Vec3(0.0, 0.0, 0.0), 0.03, 2.2, 0.30, 0.70, 0.55);
    const std::size_t orb1 = pushBall(sim::Vec3(0.0, 11.0, 10.0), sim::Vec3(13.0, 0.0, 0.0), 1.1, 0.72, 0.75, 0.22, 0.10);
    const std::size_t orb2 = pushBall(sim::Vec3(0.0, 2.0, 10.0), sim::Vec3(-12.0, 0.0, 0.0), 1.1, 0.72, 0.75, 0.22, 0.10);
    bodies[orb1].angularVelocity = sim::Vec3(0.0, 6.0, 0.0);
    bodies[orb2].angularVelocity = sim::Vec3(0.0, -5.5, 0.0);
    addJoint(center, orb1, 4.5, 0.74, 0.08);
    addJoint(center, orb2, 4.5, 0.74, 0.08);

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 6; ++x) {
            const double rx = -3.5 + static_cast<double>(x) * 1.4;
            const double ry = 10.0 + static_cast<double>(y) * 1.25;
            const double rz = (x % 2 == 0) ? 1.4 : -1.4;
            const double r = (x + y) % 3 == 0 ? 0.28 : ((x + y) % 3 == 1 ? 0.45 : 0.62);
            const double invMass = (x + y) % 4 == 0 ? 1.8 : 0.95;
            const double vx = (x % 2 == 0) ? 3.5 : -3.5;
            const double vz = (y % 2 == 0) ? -2.8 : 2.8;
            const std::size_t idx = pushBall(
                sim::Vec3(rx, ry, rz),
                sim::Vec3(vx, -0.4 * static_cast<double>(y), vz),
                invMass, r, 0.72, 0.26, 0.12);
            bodies[idx].angularVelocity = sim::Vec3(2.0 + 0.2 * static_cast<double>(x), 1.1, 0.6);
        }
    }

    for (int i = 0; i < 12; ++i) {
        const double y = 2.0 + static_cast<double>(i % 6) * 1.5;
        const double z = -6.0 + static_cast<double>(i / 2);
        const double speed = 42.0 + static_cast<double>(i) * 2.4;
        const std::size_t a = pushBall(
            sim::Vec3(-30.0 - static_cast<double>(i), y, z),
            sim::Vec3(speed, 0.0, 0.5 * static_cast<double>((i % 3) - 1)),
            2.2, 0.18, 0.88, 0.10, 0.06);
        const std::size_t b = pushBall(
            sim::Vec3(30.0 + static_cast<double>(i), y + 0.8, -z),
            sim::Vec3(-speed, 0.0, -0.5 * static_cast<double>((i % 3) - 1)),
            2.2, 0.18, 0.88, 0.10, 0.06);
        bodies[a].angularVelocity = sim::Vec3(8.0, 0.0, 3.0);
        bodies[b].angularVelocity = sim::Vec3(-8.0, 0.0, -3.0);
    }

    pushBall(sim::Vec3(-22.0, -2.5, -12.0), sim::Vec3(18.0, 0.0, 7.0), 0.05, 2.8, 0.40, 0.70, 0.45);
    pushBall(sim::Vec3(22.0, -2.0, 12.0), sim::Vec3(-19.0, 0.0, -7.5), 0.05, 2.8, 0.40, 0.70, 0.45);

    sim::World world(std::move(bodies)); // Default params
    world.params().jointIterations = 28;
    world.params().collisionIterations = 2;
    for (const auto& joint : joints) {
        world.addDistanceJoint(joint);
    }

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
    glfwSwapInterval(0);

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
    double fpsElapsed = 0.0;
    int fpsFrames = 0;
    constexpr double fpsUpdateInterval = 0.5;
    constexpr const char* kWindowTitle = "physics3d";

    std::vector<glm::mat4> models;
    std::vector<glm::vec3> renderPositions;

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

        fpsElapsed += frameTime;
        ++fpsFrames;
        if (fpsElapsed >= fpsUpdateInterval) {
            const double fps = static_cast<double>(fpsFrames) / fpsElapsed;
            char title[128];
            std::snprintf(title, sizeof(title), "%s - FPS: %.1f", kWindowTitle, fps);
            glfwSetWindowTitle(window, title);
            fpsElapsed = 0.0;
            fpsFrames = 0;
        }

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
        renderPositions.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const sim::Vec3 pSim = bs[i].prevPosition + (bs[i].position - bs[i].prevPosition) * alpha;

            const glm::vec3 pos(static_cast<float>(pSim.x), static_cast<float>(pSim.y), static_cast<float>(pSim.z));
            const auto r = static_cast<float>(bs[i].radius);
            renderPositions[i] = pos;

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

        DebugOverlay::TargetHud targetHud{};
        {
            const glm::vec3 rayOrigin = cam.pos;
            const glm::vec3 rayDir = glm::normalize(camFwd);

            int bestIdx = -1;
            float bestT = std::numeric_limits<float>::infinity();
            for (std::size_t i = 0; i < n; ++i) {
                float t = 0.0f;
                if (!raySphereHit(rayOrigin, rayDir, renderPositions[i], static_cast<float>(bs[i].radius), t)) {
                    continue;
                }
                if (t < bestT) {
                    bestT = t;
                    bestIdx = static_cast<int>(i);
                }
            }

            if (bestIdx >= 0) {
                const std::size_t i = static_cast<std::size_t>(bestIdx);
                const glm::vec3 labelPos = renderPositions[i] + glm::vec3(0.0f, static_cast<float>(bs[i].radius) * 1.6f, 0.0f);
                float sx = 0.0f;
                float sy = 0.0f;
                if (worldToScreen(labelPos, view, proj, g_fb.w, g_fb.h, sx, sy)) {
                    targetHud.visible = true;
                    targetHud.xPx = sx;
                    targetHud.yPx = sy;
                    targetHud.restitution = static_cast<float>(bs[i].restitution);
                    targetHud.staticFriction = static_cast<float>(bs[i].staticFriction);
                    targetHud.dynamicFriction = static_cast<float>(bs[i].dynamicFriction);
                }
            }
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        overlay.draw(g_fb.w, g_fb.h, paused, targetHud);
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

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <utility> // std::move
#include <cmath>
#include "ui/DebugOverlay.h"
#include <vector>

#include "sim/World.h"

static int g_fbw = 0;
static int g_fbh = 0;


struct Camera {
    glm::vec3 pos{0.0f, 0.0f, 30.0f};
    float yaw   = -glm::half_pi<float>(); // looking toward -Z by default
    float pitch = 0.0f;

    float moveSpeed = 20.0f; // units/sec
    float lookSpeed = 0.0025f;
};

static void clampPitch(Camera& c) {
    constexpr float lim = glm::radians(89.0f);
    c.pitch = std::clamp(c.pitch, -lim, lim);
}

static glm::vec3 forwardDir(const Camera& c) {
    const float cy = std::cos(c.yaw);
    const float sy = std::sin(c.yaw);
    const float cp = std::cos(c.pitch);
    const float sp = std::sin(c.pitch);
    return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

static glm::vec3 rightDir(const Camera& c) {
    return glm::normalize(glm::cross(forwardDir(c), glm::vec3(0.0f, 1.0f, 0.0f)));
}


static void APIENTRY glDebugCallback(
    const GLenum source, const GLenum type, const GLuint id, const GLenum severity,
    const GLsizei length, const GLchar* message, const void* userParam)
{
    (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
    std::cerr << "[GL DEBUG] " << message << "\n";
}

static GLuint compileShader(const GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile failed:\n" << log << "\n";
    }
    return s;
}

static GLuint linkProgram(const GLuint vs, const GLuint fs) {
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::cerr << "Program link failed:\n" << log << "\n";
    }
    return p;
}

// Vertex shader (mesh + instancing)
static auto kVert = R"GLSL(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Instance model matrix columns
layout (location = 2) in vec4 iM0;
layout (location = 3) in vec4 iM1;
layout (location = 4) in vec4 iM2;
layout (location = 5) in vec4 iM3;

uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    vec4 worldPos4 = model * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;

    mat3 normalMat = transpose(inverse(mat3(model)));
    vWorldNormal = normalize(normalMat * aNormal);

    gl_Position = uProj * uView * worldPos4;
}
)GLSL";

// Fragment shader (basic Lambert lighting)
static auto kFrag = R"GLSL(
#version 330 core

in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform vec3 uLightDir;   // world space (direction *towards* the light)
uniform vec3 uBaseColor;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vWorldNormal);
    vec3 l = normalize(uLightDir);

    float diff = max(0.0, dot(n, l));
    vec3 col = uBaseColor * (uAmbient + (1.0 - uAmbient) * diff);

    FragColor = vec4(col, 1.0);
}
)GLSL";

static void buildSphereMesh(
    int stacks, int slices, // NOLINT
    std::vector<float>& outVerts, // [px,py,pz,nx,ny,nz] per vertex
    std::vector<std::uint32_t>& outIdx) // triangle indices
{
    stacks = std::max(2, stacks);
    slices = std::max(3, slices);

    outVerts.clear();
    outIdx.clear();

    // Vertex grid: (stacks+1) rings, each with (slices+1) verts (duplicate seam).
    for (int i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks); // 0..1
        const float phi = glm::pi<float>() * v; // 0..pi

        const float y = std::cos(phi);
        const float r = std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(slices); // 0..1
            const float theta = glm::two_pi<float>() * u; // 0..2pi

            const float x = r * std::cos(theta);
            const float z = r * std::sin(theta);

            // Position on unit sphere
            outVerts.push_back(x);
            outVerts.push_back(y);
            outVerts.push_back(z);

            // Normal (same as position for unit sphere)
            outVerts.push_back(x);
            outVerts.push_back(y);
            outVerts.push_back(z);
        }
    }

    const int stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const auto i0 = static_cast<std::uint32_t>(i * stride + j);
            const auto i1 = static_cast<std::uint32_t>((i + 1) * stride + j);
            const auto i2 = static_cast<std::uint32_t>((i + 1) * stride + (j + 1));
            const auto i3 = static_cast<std::uint32_t>(i * stride + (j + 1));

            // Two triangles per quad
            outIdx.push_back(i0);
            outIdx.push_back(i1);
            outIdx.push_back(i2);

            outIdx.push_back(i0);
            outIdx.push_back(i2);
            outIdx.push_back(i3);
        }
    }
}

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

    std::vector<sim::Body> bodies;
    bodies.push_back(a);
    bodies.push_back(b);

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "physics3d", nullptr, nullptr);
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
    glfwGetFramebufferSize(window, &g_fbw, &g_fbh);
    glViewport(0, 0, g_fbw, g_fbh);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, const int w, const int h) {
        g_fbw = w;
        g_fbh = h;
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
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFrag);
    const GLuint program = linkProgram(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

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


        auto& bs = world.bodies();

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
        const double alpha = paused ? 1.0 : std::clamp(accumulator / dt, 0.0, 1.0);

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
        const float aspect = (g_fbh > 0) ? (static_cast<float>(g_fbw) / static_cast<float>(g_fbh)) : 1.0f;

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
        overlay.draw(g_fbw, g_fbh, paused);
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

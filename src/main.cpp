#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>
#include <utility> // std::move
#include <vector>
#include "sim/World.h"


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

// Vertex shader
static auto kVert = R"GLSL(
#version 330 core
layout (location = 0) in vec4 aPosR; // xyz = position, w = radius

uniform mat4 uVP;
uniform mat4 uView;

// For point sizing in pixels:
uniform float uProjScaleY;   // proj[1][1]
uniform float uViewportH;    // framebuffer height in pixels

void main() {
    vec4 viewPos = uView * vec4(aPosR.xyz, 1.0);
    float z = max(0.001, -viewPos.z); // distance in front of camera

    gl_Position = uVP * vec4(aPosR.xyz, 1.0);

    // diameter in pixels
    gl_PointSize = (aPosR.w * uProjScaleY * uViewportH) / z;
}
)GLSL";

// Fragment shader
static auto kFrag = R"GLSL(
#version 330 core
out vec4 FragColor;

void main() {
    // gl_PointCoord is [0,1] across the point sprite
    vec2 p = gl_PointCoord * 2.0 - 1.0; // [-1,1]
    float r2 = dot(p, p);
    if (r2 > 1.0) discard; // circle mask

    // fake sphere normal
    float z = sqrt(1.0 - r2);
    vec3 n = normalize(vec3(p, z));

    vec3 lightDir = normalize(vec3(0.4, 0.6, 1.0));
    float diff = max(0.0, dot(n, lightDir));

    vec3 base = vec3(1.0, 1.0, 0.0); // yellow
    vec3 col = base * (0.25 + 0.75 * diff);

    FragColor = vec4(col, 1.0);
}
)GLSL";


int main() {
    // Init world
    sim::Body a{};
    a.invMass = 1.0;

    sim::Body b{};
    b.position = sim::Vec3(10.0, -10.0, 10.0);
    b.velocity = sim::Vec3(0.0, 0.0, 0.0);
    b.invMass  = 0.5;
    b.radius   = 2.0;

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

    static int g_fbw = 0, g_fbh = 0;

    glfwGetFramebufferSize(window, &g_fbw, &g_fbh);
    glViewport(0, 0, g_fbw, g_fbh);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        g_fbw = w; g_fbh = h;
        glViewport(0, 0, w, h);
    });

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFrag);
    const GLuint program = linkProgram(vs, fs);

    GLint uVP = glGetUniformLocation(program, "uVP"); // NOLINT
    GLint uView       = glGetUniformLocation(program, "uView");       // NOLINT
    GLint uProjScaleY = glGetUniformLocation(program, "uProjScaleY"); // NOLINT
    GLint uViewportH  = glGetUniformLocation(program, "uViewportH");  // NOLINT

    if (uVP < 0 || uView < 0 || uProjScaleY < 0 || uViewportH < 0) {
        std::cerr << "Missing uniform(s). Check shader names match exactly.\n";
    }


    // Delete shaders after linking
    glDeleteShader(vs);
    glDeleteShader(fs);

    std::cout << "Shader program linked: " << program << "\n";

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    // Track how many bodies the VBO is currently sized for
    std::size_t vboCapacityBodies = std::max<std::size_t>(1, world.bodies().size());

    // CPU-side staging buffer (reused each frame)
    std::vector<float> gpuVerts;

    // Configure VAO
    glBindVertexArray(vao);

    // Upload vertex data into the VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vboCapacityBodies * 4 * sizeof(float)),
        nullptr,
        GL_DYNAMIC_DRAW);

    // Tell OpenGL how to read the buffer for shader input location 0 (aPos)
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Tick loop
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

        const auto& bs = world.bodies();
        const std::size_t n = bs.size();

        if (n == 0) {
            glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glfwSwapBuffers(window);
            continue;
        }

        // If body count changed, resize the VBO
        if (n != vboCapacityBodies) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(n * 4 * sizeof(float)),
                nullptr,
                GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            vboCapacityBodies = n;
        }

        // Camera at world origin
        constexpr float cx = 0.0; // NOLINT
        constexpr float cy = 0.0; // NOLINT
        constexpr float cz = 0.0; // NOLINT

        // Camera zoom
        constexpr float viewHalfWidth = 15.0f; // NOLINT

        // Build N vertices
        gpuVerts.resize(n * 4);
        for (std::size_t i = 0; i < n; ++i) {
            gpuVerts[4*i + 0] = static_cast<float>(bs[i].position.x);
            gpuVerts[4*i + 1] = static_cast<float>(bs[i].position.y);
            gpuVerts[4*i + 2] = static_cast<float>(bs[i].position.z);
            gpuVerts[4*i + 3] = static_cast<float>(bs[i].radius);
        }

        // Upload into the existing VBO
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(gpuVerts.size() * sizeof(float)),
            gpuVerts.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // Maintain aspect ratio
        const float aspect = (g_fbh > 0) ? (static_cast<float>(g_fbw) / static_cast<float>(g_fbh)) : 1.0f;

        // Perspective camera
        const glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
        const glm::mat4 view = glm::lookAt(
            glm::vec3(cx, cy, cz + 30.0f),
            glm::vec3(cx, cy, cz),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        const glm::mat4 vp = proj * view;

        glUseProgram(program);

        glUniformMatrix4fv(uVP,   1, GL_FALSE, &vp[0][0]);
        glUniformMatrix4fv(uView, 1, GL_FALSE, &view[0][0]);

        glUniform1f(uProjScaleY, proj[1][1]); // perspective scale factor
        glUniform1f(uViewportH,  static_cast<float>(g_fbh)); // framebuffer height in pixels

        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(n));
        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

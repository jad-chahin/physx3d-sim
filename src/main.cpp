#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
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
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
    gl_PointSize = 12.0;
}
)GLSL";

// Fragment shader
static auto kFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 1.0, 0.0, 1.0); // yellow
}
)GLSL";


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

    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFrag);
    const GLuint program = linkProgram(vs, fs);

    // Delete shaders after linking
    glDeleteShader(vs);
    glDeleteShader(fs);

    std::cout << "Shader program linked: " << program << "\n";

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    // Configure VAO
    glBindVertexArray(vao);

    // Upload vertex data into the VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 2 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Tell OpenGL how to read the buffer for shader input location 0 (aPos)
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        nullptr);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
    glEnable(GL_PROGRAM_POINT_SIZE);


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

        // Center the view between the two bodies (so they stay visible)
        // const double cx = 0.5 * (bs[0].position.x + bs[1].position.x);

        // Pin camera to A
        // const double cx = bs[0].position.x;

        // Fixed world origin
        constexpr double cx = 0.0;

        // Camera zoom
        constexpr double viewHalfWidthMeters = 15.0;

        // Convert world meters -> NDC
        const float gpuVerts[] = {
            static_cast<float>((bs[0].position.x - cx) / viewHalfWidthMeters), 0.0f, 0.0f,
            static_cast<float>((bs[1].position.x - cx) / viewHalfWidthMeters), 0.0f, 0.0f
        };

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(gpuVerts), gpuVerts);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, 2);
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

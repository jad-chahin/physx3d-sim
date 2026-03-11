#include "ui/OverlayRenderer.h"
#include "ui/OverlayRendererInternal.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {

GLuint compileShader(const GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    }
    return shader;
}

GLuint linkProgram(const GLuint vs, const GLuint fs) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    }
    return program;
}

constexpr auto kUiVert = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPosPx;

uniform vec2 uViewport;

void main() {
    vec2 ndc;
    ndc.x = (aPosPx.x / uViewport.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (aPosPx.y / uViewport.y) * 2.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

constexpr auto kUiFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";

void drawLayer(const GLint uColor, const std::vector<float>& vertices, const std::array<float, 4>& color) {
    if (vertices.empty()) {
        return;
    }

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 2));
}

} // namespace

void OverlayRenderer::init() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kUiVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kUiFrag);
    program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    uViewport_ = glGetUniformLocation(program_, "uViewport");
    uColor_ = glGetUniformLocation(program_, "uColor");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OverlayRenderer::shutdown() {
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (program_) {
        glDeleteProgram(program_);
    }
    vbo_ = 0;
    vao_ = 0;
    program_ = 0;
}

void OverlayRenderer::draw(
    const int fbw,
    const int fbh,
    const bool simFrozen,
    const double simSpeed,
    const double fps,
    const PauseMenuHud& pauseMenu,
    const TargetHud& targetHud,
    const float uiScale,
    const bool showHud,
    const bool showCrosshair,
    const std::vector<std::string>& hudDebugLines) const
{
    std::vector<float> screenDim;
    screenDim.reserve(24);
    std::vector<float> panelFill;
    panelFill.reserve(600);
    std::vector<float> panelFrame;
    panelFrame.reserve(600);
    std::vector<float> accentFill;
    accentFill.reserve(600);
    std::vector<float> textPrimary;
    textPrimary.reserve(9000);
    std::vector<float> textMuted;
    textMuted.reserve(9000);
    std::vector<float> textAccent;
    textAccent.reserve(9000);
    std::vector<float> textWarning;
    textWarning.reserve(3000);
    std::vector<float> statusText;
    statusText.reserve(2000);
    std::vector<float> crosshair;
    crosshair.reserve(24);
    std::vector<float> popupBg;
    std::vector<float> popupFrame;
    std::vector<float> popupText;
    popupText.reserve(1200);

    const overlay_renderer::Geometry geometry{
        static_cast<float>(fbw),
        static_cast<float>(fbh),
        std::clamp(uiScale, 0.75f, 2.0f)};
    const overlay_renderer::Buffers buffers{
        screenDim,
        panelFill,
        panelFrame,
        accentFill,
        textPrimary,
        textMuted,
        textAccent,
        textWarning,
        statusText,
        crosshair,
        popupBg,
        popupFrame,
        popupText};

    if (pauseMenu.visible) {
        overlay_renderer::drawPauseMenu(pauseMenu, geometry, buffers);
    } else if (showHud) {
        overlay_renderer::drawHud(geometry, simFrozen, simSpeed, fps, hudDebugLines, buffers);
    }

    if (!pauseMenu.visible && showCrosshair) {
        overlay_renderer::drawCrosshair(geometry, buffers);
    }
    if (!pauseMenu.visible) {
        overlay_renderer::drawTargetPopup(targetHud, geometry, buffers);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glUniform2f(uViewport_, static_cast<float>(fbw), static_cast<float>(fbh));
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    drawLayer(uColor_, screenDim, {0.01f, 0.03f, 0.05f, pauseMenu.visible ? 0.42f : 0.62f});
    drawLayer(
        uColor_,
        panelFill,
        {pauseMenu.visible ? 0.03f : 0.05f,
         pauseMenu.visible ? 0.05f : 0.07f,
         pauseMenu.visible ? 0.07f : 0.10f,
         pauseMenu.visible ? 0.56f : 0.90f});
    drawLayer(uColor_, panelFrame, {0.18f, 0.24f, 0.30f, pauseMenu.visible ? 0.78f : 0.98f});
    drawLayer(uColor_, accentFill, {0.23f, 0.70f, 0.92f, pauseMenu.visible ? 0.72f : 0.95f});
    drawLayer(uColor_, popupBg, {0.05f, 0.07f, 0.10f, 0.94f});
    drawLayer(uColor_, popupFrame, {0.18f, 0.24f, 0.30f, 0.98f});
    drawLayer(uColor_, textPrimary, {1.0f, 1.0f, 1.0f, 1.0f});
    drawLayer(uColor_, textMuted, {0.74f, 0.80f, 0.86f, 0.98f});
    drawLayer(uColor_, textAccent, {0.37f, 0.81f, 0.97f, 1.0f});
    drawLayer(uColor_, textWarning, {1.0f, 0.62f, 0.31f, 1.0f});
    drawLayer(uColor_, popupText, {0.95f, 0.97f, 1.0f, 1.0f});
    drawLayer(
        uColor_,
        statusText,
        {simFrozen ? 1.0f : 0.48f,
         simFrozen ? 0.42f : 0.88f,
         simFrozen ? 0.42f : 0.54f,
         1.0f});
    drawLayer(uColor_, crosshair, {0.32f, 0.85f, 0.95f, 1.0f});

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

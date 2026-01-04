//
// Created by jchah on 2026-01-04.
//

#include "ui/DebugOverlay.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>


static GLuint compileShader(const GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
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
    }
    return p;
}

// ---------------- UI shaders ----------------
static auto kUiVert = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPosPx;   // pixel coords, origin = top-left

uniform vec2 uViewport;                // (fbw, fbh)

void main() {
    vec2 ndc;
    ndc.x = (aPosPx.x / uViewport.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (aPosPx.y / uViewport.y) * 2.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

static auto kUiFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

// ---------------- Tiny 5x7 font ----------------
static constexpr int kFontW = 5;
static constexpr int kFontH = 7;
static constexpr int kAdvance = 6; // 5 pixels + 1 spacing

static const std::array<std::uint8_t, 7>& glyph5x7(char c) {
    static constexpr std::array<std::uint8_t, 7> SP = {0,0,0,0,0,0,0};

    static constexpr std::array<std::uint8_t, 7> A = {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> C = {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> D = {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
    static constexpr std::array<std::uint8_t, 7> E = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
    static constexpr std::array<std::uint8_t, 7> G = {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> K = {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
    static constexpr std::array<std::uint8_t, 7> L = {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
    static constexpr std::array<std::uint8_t, 7> M = {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> O = {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> P = {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000};
    static constexpr std::array<std::uint8_t, 7> Q = {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
    static constexpr std::array<std::uint8_t, 7> S = {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110};
    static constexpr std::array<std::uint8_t, 7> T = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
    static constexpr std::array<std::uint8_t, 7> U = {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> V = {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100};
    static constexpr std::array<std::uint8_t, 7> W = {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001};

    static constexpr std::array<std::uint8_t, 7> COLON = {0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000};
    static constexpr std::array<std::uint8_t, 7> PLUS  = {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000};

    switch (c) {
        case 'A': return A;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'G': return G;
        case 'K': return K;
        case 'L': return L;
        case 'M': return M;
        case 'O': return O;
        case 'P': return P;
        case 'Q': return Q;
        case 'S': return S;
        case 'T': return T;
        case 'U': return U;
        case 'V': return V;
        case 'W': return W;
        case ':': return COLON;
        case '+': return PLUS;
        default:  return SP;
    }
}

static void pushQuadPx(std::vector<float>& v, const float x0, const float y0, const float x1, const float y1) {
    v.push_back(x0); v.push_back(y0);
    v.push_back(x1); v.push_back(y0);
    v.push_back(x1); v.push_back(y1);

    v.push_back(x0); v.push_back(y0);
    v.push_back(x1); v.push_back(y1);
    v.push_back(x0); v.push_back(y1);
}

static float measureLinePx(const std::string& s, float scalePx) { // NOLINT
    int count = 0;
    for (const char c : s) {
        if (c == '\n') break;
        (void)c;
        count += kAdvance;
    }
    return static_cast<float>(count) * scalePx;
}

static void appendTextPx(std::vector<float>& out, const float x, const float y, float scalePx, const std::string& s) { // NOLINT
    const float cell = scalePx;
    float penX = x;
    float penY = y;

    for (const char c : s) {
        if (c == '\n') {
            penX = x;
            penY += static_cast<float>(kFontH + 2) * cell;
            continue;
        }

        const auto& g = glyph5x7(c);
        for (int row = 0; row < kFontH; ++row) {
            const std::uint8_t bits = g[row];
            for (int col = 0; col < kFontW; ++col) {
                if (const bool on = ((bits >> (kFontW - 1 - col)) & 1U) != 0U; !on) continue;

                const float x0 = penX + static_cast<float>(col) * cell;
                const float y0 = penY + static_cast<float>(row) * cell;
                const float x1 = x0 + cell;
                const float y1 = y0 + cell;
                pushQuadPx(out, x0, y0, x1, y1);
            }
        }

        penX += static_cast<float>(kAdvance) * cell;
    }
}

void DebugOverlay::init() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kUiVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kUiFrag);
    program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    uViewport_ = glGetUniformLocation(program_, "uViewport");
    uColor_    = glGetUniformLocation(program_, "uColor");

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

void DebugOverlay::shutdown() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    vbo_ = 0;
    vao_ = 0;
    program_ = 0;
}

void DebugOverlay::draw(const int fbw, const int fbh, const bool paused) const {
    const std::string controls =
        "ESC: TOGGLE MOUSE\n"
        "SPACE: PAUSE\n"
        "WASD + QE: MOVE\n"
        "MOUSE: LOOK";

    constexpr float scalePx = 2.0f;
    constexpr float marginPx = 12.0f;

    const float blockW = measureLinePx("ESC: TOGGLE MOUSE", scalePx);
    float x = static_cast<float>(fbw) - marginPx - blockW;
    float y = marginPx;

    std::vector<float> verts;
    verts.reserve(8000);

    std::size_t pausedVertsCount = 0;
    if (paused) {
        appendTextPx(verts, x, y, scalePx, "PAUSED");
        y += static_cast<float>(kFontH + 4) * scalePx;
        pausedVertsCount = verts.size();
    }

    appendTextPx(verts, x, y, scalePx, controls);

    glDisable(GL_DEPTH_TEST);

    glUseProgram(program_);
    glUniform2f(uViewport_, static_cast<float>(fbw), static_cast<float>(fbh));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);

    if (pausedVertsCount > 0) {
        glUniform3f(uColor_, 1.0f, 0.2f, 0.2f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pausedVertsCount / 2));
    }

    glUniform3f(uColor_, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(pausedVertsCount / 2),
                 static_cast<GLsizei>((verts.size() - pausedVertsCount) / 2));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

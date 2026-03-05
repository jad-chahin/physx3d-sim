//
// Created by jchah on 2026-01-04.
//

#include "ui/DebugOverlay.h"
#include <array>
#include <cstdint>
#include <cstdio>
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
    static constexpr std::array<std::uint8_t, 7> B = {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
    static constexpr std::array<std::uint8_t, 7> C = {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> D = {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
    static constexpr std::array<std::uint8_t, 7> E = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
    static constexpr std::array<std::uint8_t, 7> G = {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> K = {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
    static constexpr std::array<std::uint8_t, 7> L = {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
    static constexpr std::array<std::uint8_t, 7> M = {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> N = {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> O = {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> P = {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000};
    static constexpr std::array<std::uint8_t, 7> Q = {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
    static constexpr std::array<std::uint8_t, 7> R = {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
    static constexpr std::array<std::uint8_t, 7> S = {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110};
    static constexpr std::array<std::uint8_t, 7> T = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
    static constexpr std::array<std::uint8_t, 7> U = {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> V = {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100};
    static constexpr std::array<std::uint8_t, 7> W = {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001};

    static constexpr std::array<std::uint8_t, 7> N0 = {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> N1 = {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
    static constexpr std::array<std::uint8_t, 7> N2 = {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111};
    static constexpr std::array<std::uint8_t, 7> N3 = {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110};
    static constexpr std::array<std::uint8_t, 7> N4 = {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010};
    static constexpr std::array<std::uint8_t, 7> N5 = {0b11111,0b10000,0b10000,0b11110,0b00001,0b00001,0b11110};
    static constexpr std::array<std::uint8_t, 7> N6 = {0b01110,0b10000,0b10000,0b11110,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> N7 = {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000};
    static constexpr std::array<std::uint8_t, 7> N8 = {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> N9 = {0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b01110};

    static constexpr std::array<std::uint8_t, 7> COLON = {0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000};
    static constexpr std::array<std::uint8_t, 7> PLUS  = {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000};
    static constexpr std::array<std::uint8_t, 7> MINUS = {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000};
    static constexpr std::array<std::uint8_t, 7> EQUAL = {0b00000,0b11111,0b00000,0b11111,0b00000,0b00000,0b00000};
    static constexpr std::array<std::uint8_t, 7> DOT   = {0b00000,0b00000,0b00000,0b00000,0b00000,0b00110,0b00110};
    static constexpr std::array<std::uint8_t, 7> UNDERSCORE = {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b11111};

    switch (c) {
        case 'A': return A;
        case 'B': return B;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'G': return G;
        case 'K': return K;
        case 'L': return L;
        case 'M': return M;
        case 'N': return N;
        case 'O': return O;
        case 'P': return P;
        case 'Q': return Q;
        case 'R': return R;
        case 'S': return S;
        case 'T': return T;
        case 'U': return U;
        case 'V': return V;
        case 'W': return W;
        case '0': return N0;
        case '1': return N1;
        case '2': return N2;
        case '3': return N3;
        case '4': return N4;
        case '5': return N5;
        case '6': return N6;
        case '7': return N7;
        case '8': return N8;
        case '9': return N9;
        case ':': return COLON;
        case '+': return PLUS;
        case '-': return MINUS;
        case '=': return EQUAL;
        case '.': return DOT;
        case '_': return UNDERSCORE;
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

void DebugOverlay::draw(const int fbw, const int fbh, const bool paused, const TargetHud& targetHud) const {
    const std::string controls =
        "ESC: TOGGLE MOUSE\n"
        "SPACE: PAUSE\n"
        "-: SPEED DOWN\n"
        "=: SPEED UP\n"
        "1: SPEED RESET\n"
        "WASD + QE: MOVE\n"
        "MOUSE: LOOK";

    constexpr float scalePx = 2.0f;
    constexpr float marginPx = 12.0f;

    const float blockW = measureLinePx("ESC: TOGGLE MOUSE", scalePx);
    float x = static_cast<float>(fbw) - marginPx - blockW;
    float y = marginPx;

    std::vector<float> verts;
    verts.reserve(8000);
    std::vector<float> crosshair;
    crosshair.reserve(24);

    std::size_t pausedVertsCount = 0;
    if (paused) {
        appendTextPx(verts, x, y, scalePx, "PAUSED");
        y += static_cast<float>(kFontH + 4) * scalePx;
        pausedVertsCount = verts.size();
    }

    appendTextPx(verts, x, y, scalePx, controls);

    std::vector<float> popupBg;
    if (targetHud.visible) {
        char popup[160];
        std::snprintf(
            popup, sizeof(popup),
            "E:%.2f\nS:%.2f\nD:%.2f",
            targetHud.restitution, targetHud.staticFriction, targetHud.dynamicFriction);

        const float popupW = measureLinePx("D:00.00", scalePx) + 8.0f;
        constexpr float popupH = static_cast<float>((kFontH + 2) * 3) * scalePx + 8.0f;
        const float px = targetHud.xPx - popupW * 0.5f;
        const float py = targetHud.yPx - popupH - 12.0f;

        pushQuadPx(popupBg, px, py, px + popupW, py + popupH);
        appendTextPx(verts, px + 4.0f, py + 4.0f, scalePx, popup);
    }

    // Center crosshair for easier target selection.
    const float cx = static_cast<float>(fbw) * 0.5f;
    const float cy = static_cast<float>(fbh) * 0.5f;
    constexpr float crossHalf = 7.0f;
    constexpr float crossThick = 1.0f;
    pushQuadPx(crosshair, cx - crossHalf, cy - crossThick, cx + crossHalf, cy + crossThick);
    pushQuadPx(crosshair, cx - crossThick, cy - crossHalf, cx + crossThick, cy + crossHalf);

    glDisable(GL_DEPTH_TEST);

    glUseProgram(program_);
    glUniform2f(uViewport_, static_cast<float>(fbw), static_cast<float>(fbh));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);

    if (!popupBg.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(popupBg.size() * sizeof(float)), popupBg.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.08f, 0.08f, 0.08f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(popupBg.size() / 2));

        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);
    }

    if (pausedVertsCount > 0) {
        glUniform3f(uColor_, 1.0f, 0.2f, 0.2f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pausedVertsCount / 2));
    }

    glUniform3f(uColor_, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(pausedVertsCount / 2),
                 static_cast<GLsizei>((verts.size() - pausedVertsCount) / 2));

    if (!crosshair.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(crosshair.size() * sizeof(float)), crosshair.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.85f, 0.95f, 0.30f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(crosshair.size() / 2));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

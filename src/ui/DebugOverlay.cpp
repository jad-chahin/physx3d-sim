//
// Created by jchah on 2026-01-04.
//

#include "ui/DebugOverlay.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
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
    static constexpr std::array<std::uint8_t, 7> F = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000};
    static constexpr std::array<std::uint8_t, 7> G = {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110};
    static constexpr std::array<std::uint8_t, 7> H = {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> I = {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110};
    static constexpr std::array<std::uint8_t, 7> J = {0b00001,0b00001,0b00001,0b00001,0b10001,0b10001,0b01110};
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
    static constexpr std::array<std::uint8_t, 7> X = {0b10001,0b01010,0b00100,0b00100,0b01010,0b10001,0b10001};
    static constexpr std::array<std::uint8_t, 7> Y = {0b10001,0b01010,0b00100,0b00100,0b00100,0b00100,0b00100};
    static constexpr std::array<std::uint8_t, 7> Z = {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111};

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
        case 'F': return F;
        case 'G': return G;
        case 'H': return H;
        case 'I': return I;
        case 'J': return J;
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
        case 'X': return X;
        case 'Y': return Y;
        case 'Z': return Z;
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

static void pushFramePx(
    std::vector<float>& v,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float thickness)
{
    pushQuadPx(v, x0, y0, x1, y0 + thickness); // top
    pushQuadPx(v, x0, y1 - thickness, x1, y1); // bottom
    pushQuadPx(v, x0, y0 + thickness, x0 + thickness, y1 - thickness); // left
    pushQuadPx(v, x1 - thickness, y0 + thickness, x1, y1 - thickness); // right
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

void DebugOverlay::draw(
    const int fbw,
    const int fbh,
    const bool simFrozen,
    const double simSpeed,
    const double fps,
    const PauseMenuHud& pauseMenu,
    const TargetHud& targetHud) const
{
    constexpr float scalePx = 2.0f;
    constexpr float hudHintScalePx = scalePx;
    constexpr float settingsScalePx = 2.10f;
    constexpr float controlsScalePx = 2.30f;
    constexpr float titleScalePx = 3.0f;

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

    const float w = static_cast<float>(fbw);
    const float h = static_cast<float>(fbh);

    if (pauseMenu.visible) {
        pushQuadPx(panelFill, 0.0f, 0.0f, w, h);

        const float cardW = std::min(w * 0.82f, 1120.0f);
        const float cardH = std::min(h * 0.84f, 900.0f);
        const float cardX0 = (w - cardW) * 0.5f;
        const float cardY0 = (h - cardH) * 0.5f;
        const float cardX1 = cardX0 + cardW;
        const float cardY1 = cardY0 + cardH;

        pushQuadPx(panelFill, cardX0, cardY0, cardX1, cardY1);
        pushFramePx(panelFrame, cardX0, cardY0, cardX1, cardY1, 2.0f);
        pushQuadPx(accentFill, cardX0, cardY0, cardX1, cardY0 + 4.0f);

        const std::string title = "PAUSE MENU";
        const std::string resume = "ESC: RESUME";
        appendTextPx(
            textAccent,
            w * 0.5f - measureLinePx(title, titleScalePx) * 0.5f,
            cardY0 + 20.0f,
            titleScalePx,
            title);
        appendTextPx(
            textMuted,
            w * 0.5f - measureLinePx(resume, scalePx) * 0.5f,
            cardY0 + 62.0f,
            scalePx,
            resume);

        std::string rebindLine = "UP DOWN TO SELECT\nENTER TO REBIND";
        if (pauseMenu.awaitingBind && !pauseMenu.pendingAction.empty()) {
            rebindLine = "PRESS KEY FOR " + pauseMenu.pendingAction;
            appendTextPx(
                textWarning,
                w * 0.5f - measureLinePx("ESC: CANCEL", scalePx) * 0.5f,
                cardY0 + 90.0f,
                scalePx,
                "ESC: CANCEL");
        }
        appendTextPx(
            textPrimary,
            w * 0.5f - measureLinePx(rebindLine, scalePx) * 0.5f,
            cardY0 + 118.0f,
            scalePx,
            rebindLine);

        const float sectionX0 = cardX0 + 22.0f;
        const float sectionX1 = cardX1 - 22.0f;

        const float settingsY0 = cardY0 + 176.0f;
        const float settingsY1 = settingsY0 + std::min(170.0f, (cardY1 - settingsY0 - 26.0f) * 0.38f);
        pushQuadPx(panelFill, sectionX0, settingsY0, sectionX1, settingsY1);
        pushFramePx(panelFrame, sectionX0, settingsY0, sectionX1, settingsY1, 1.5f);
        pushQuadPx(accentFill, sectionX0, settingsY0, sectionX1, settingsY0 + 3.0f);
        appendTextPx(textAccent, sectionX0 + 12.0f, settingsY0 + 12.0f, 2.4f, "SETTINGS");

        const float settingsStartY = settingsY0 + 44.0f;
        const float settingsStep = static_cast<float>(kFontH + 3) * settingsScalePx;
        const float maxSettings = std::floor((settingsY1 - settingsStartY - 10.0f) / settingsStep);
        const std::size_t visibleSettings = std::min<std::size_t>(
            pauseMenu.settingLines.size(),
            static_cast<std::size_t>(std::max(0.0f, maxSettings)));
        for (std::size_t i = 0; i < visibleSettings; ++i) {
            appendTextPx(
                textPrimary,
                sectionX0 + 12.0f,
                settingsStartY + static_cast<float>(i) * settingsStep,
                settingsScalePx,
                pauseMenu.settingLines[i]);
        }

        const float controlsY0 = settingsY1 + 14.0f;
        const float controlsY1 = cardY1 - 16.0f;
        pushQuadPx(panelFill, sectionX0, controlsY0, sectionX1, controlsY1);
        pushFramePx(panelFrame, sectionX0, controlsY0, sectionX1, controlsY1, 1.5f);
        pushQuadPx(accentFill, sectionX0, controlsY0, sectionX1, controlsY0 + 3.0f);
        appendTextPx(textAccent, sectionX0 + 12.0f, controlsY0 + 12.0f, 2.4f, "CONTROLS");

        const float controlsStartY = controlsY0 + 46.0f;
        const float controlsStep = static_cast<float>(kFontH + 4) * controlsScalePx;
        const float maxControls = std::floor((controlsY1 - controlsStartY - 12.0f) / controlsStep);
        const std::size_t totalControls = pauseMenu.controlLines.size();
        const std::size_t visibleControls = std::min<std::size_t>(
            totalControls,
            static_cast<std::size_t>(std::max(0.0f, maxControls)));

        std::size_t firstControl = 0;
        if (visibleControls > 0 && totalControls > visibleControls) {
            const int clampedSelected = std::clamp(
                pauseMenu.selectedControlIndex,
                0,
                static_cast<int>(totalControls - 1));
            const std::size_t selected = static_cast<std::size_t>(clampedSelected);
            if (selected >= visibleControls) {
                firstControl = selected - visibleControls + 1;
            }
            const std::size_t maxFirst = totalControls - visibleControls;
            if (firstControl > maxFirst) {
                firstControl = maxFirst;
            }
        }

        for (std::size_t i = 0; i < visibleControls; ++i) {
            const std::size_t lineIdx = firstControl + i;
            appendTextPx(
                textPrimary,
                sectionX0 + 12.0f,
                controlsStartY + static_cast<float>(i) * controlsStep,
                controlsScalePx,
                pauseMenu.controlLines[lineIdx]);
        }
    } else {
        const float hudX0 = 16.0f;
        const float hudY0 = 16.0f;
        const float hudX1 = hudX0 + 252.0f;
        const float hudY1 = hudY0 + 92.0f;
        const float hudTextX = hudX0 + 10.0f;
        const float hudTextY0 = hudY0 + 10.0f;
        constexpr float hudRowStep = 20.0f;
        char speedLine[64];
        std::snprintf(speedLine, sizeof(speedLine), "SPEED: %.2fX", simSpeed);
        char fpsLine[64];
        std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);
        pushQuadPx(panelFill, hudX0, hudY0, hudX1, hudY1);
        pushFramePx(panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);
        pushQuadPx(accentFill, hudX0, hudY0, hudX1, hudY0 + 3.0f);

        appendTextPx(
            statusText,
            hudTextX,
            hudTextY0,
            scalePx,
            simFrozen ? "WORLD: FROZEN" : "WORLD: RUNNING");
        appendTextPx(textMuted, hudTextX, hudTextY0 + hudRowStep, hudHintScalePx, "ESC: MENU");
        appendTextPx(textPrimary, hudTextX, hudTextY0 + hudRowStep * 2.0f, hudHintScalePx, speedLine);
        appendTextPx(textPrimary, hudTextX, hudTextY0 + hudRowStep * 3.0f, hudHintScalePx, fpsLine);
    }

    // Center crosshair with separated ticks.
    if (!pauseMenu.visible) {
        const float cx = w * 0.5f;
        const float cy = h * 0.5f;
        constexpr float gap = 5.0f;
        constexpr float len = 8.0f;
        constexpr float thick = 1.0f;
        pushQuadPx(crosshair, cx - len - gap, cy - thick, cx - gap, cy + thick);
        pushQuadPx(crosshair, cx + gap, cy - thick, cx + len + gap, cy + thick);
        pushQuadPx(crosshair, cx - thick, cy - len - gap, cx + thick, cy - gap);
        pushQuadPx(crosshair, cx - thick, cy + gap, cx + thick, cy + len + gap);
        pushQuadPx(crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
    }

    if (targetHud.visible && !pauseMenu.visible) {
        char popup[160];
        std::snprintf(
            popup, sizeof(popup),
            "TARGET\nE:%.2f\nS:%.2f\nD:%.2f",
            targetHud.restitution, targetHud.staticFriction, targetHud.dynamicFriction);

        const float popupW = measureLinePx("TARGET", scalePx) + 56.0f;
        constexpr float popupH = static_cast<float>((kFontH + 2) * 4) * scalePx + 12.0f;
        const float px = targetHud.xPx - popupW * 0.5f;
        const float py = targetHud.yPx - popupH - 12.0f;

        pushQuadPx(popupBg, px, py, px + popupW, py + popupH);
        pushFramePx(popupFrame, px, py, px + popupW, py + popupH, 1.5f);
        pushQuadPx(accentFill, px, py, px + popupW, py + 3.0f);
        appendTextPx(popupText, px + 8.0f, py + 6.0f, scalePx, popup);
    }

    glDisable(GL_DEPTH_TEST);

    glUseProgram(program_);
    glUniform2f(uViewport_, static_cast<float>(fbw), static_cast<float>(fbh));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    if (!panelFill.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(panelFill.size() * sizeof(float)), panelFill.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, pauseMenu.visible ? 0.01f : 0.05f, pauseMenu.visible ? 0.01f : 0.07f, pauseMenu.visible ? 0.02f : 0.10f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(panelFill.size() / 2));
    }
    if (!panelFrame.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(panelFrame.size() * sizeof(float)), panelFrame.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.18f, 0.24f, 0.30f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(panelFrame.size() / 2));
    }
    if (!accentFill.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(accentFill.size() * sizeof(float)), accentFill.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.23f, 0.70f, 0.92f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(accentFill.size() / 2));
    }

    if (!popupBg.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(popupBg.size() * sizeof(float)), popupBg.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.05f, 0.07f, 0.10f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(popupBg.size() / 2));
    }
    if (!popupFrame.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(popupFrame.size() * sizeof(float)), popupFrame.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.18f, 0.24f, 0.30f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(popupFrame.size() / 2));
    }

    if (!textPrimary.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textPrimary.size() * sizeof(float)), textPrimary.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textPrimary.size() / 2));
    }
    if (!textMuted.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textMuted.size() * sizeof(float)), textMuted.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.74f, 0.80f, 0.86f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textMuted.size() / 2));
    }
    if (!textAccent.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textAccent.size() * sizeof(float)), textAccent.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.37f, 0.81f, 0.97f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textAccent.size() / 2));
    }
    if (!textWarning.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(textWarning.size() * sizeof(float)), textWarning.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 1.0f, 0.62f, 0.31f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textWarning.size() / 2));
    }
    if (!popupText.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(popupText.size() * sizeof(float)), popupText.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.95f, 0.97f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(popupText.size() / 2));
    }
    if (!statusText.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(statusText.size() * sizeof(float)), statusText.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, simFrozen ? 1.0f : 0.48f, simFrozen ? 0.42f : 0.88f, simFrozen ? 0.42f : 0.54f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(statusText.size() / 2));
    }

    if (!crosshair.empty()) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(crosshair.size() * sizeof(float)), crosshair.data(), GL_DYNAMIC_DRAW);
        glUniform3f(uColor_, 0.32f, 0.85f, 0.95f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(crosshair.size() / 2));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

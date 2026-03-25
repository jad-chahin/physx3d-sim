#ifndef PHYSICS3D_UI_OVERLAYRENDERERDRAWSHARED_H
#define PHYSICS3D_UI_OVERLAYRENDERERDRAWSHARED_H

#include "ui/PauseMenuLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace overlay_renderer::draw_shared {

inline constexpr int kFontW = 5;
inline constexpr int kFontH = ui::pause_menu_layout::kFontH;
inline constexpr int kAdvance = ui::pause_menu_layout::kAdvance;
inline constexpr float kBaseScalePx = ui::pause_menu_layout::kBaseScalePx;
inline constexpr float kTitleScalePx = ui::pause_menu_layout::kTitleScalePx;

inline const std::array<std::uint8_t, 7>& glyph5x7(const char c) {
    static constexpr std::array<std::uint8_t, 7> SP = {0, 0, 0, 0, 0, 0, 0};
    static constexpr std::array<std::uint8_t, 7> A = {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    static constexpr std::array<std::uint8_t, 7> B = {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
    static constexpr std::array<std::uint8_t, 7> C = {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> D = {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
    static constexpr std::array<std::uint8_t, 7> E = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
    static constexpr std::array<std::uint8_t, 7> F = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
    static constexpr std::array<std::uint8_t, 7> G = {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> H = {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    static constexpr std::array<std::uint8_t, 7> I = {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    static constexpr std::array<std::uint8_t, 7> J = {0b00001, 0b00001, 0b00001, 0b00001, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> K = {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
    static constexpr std::array<std::uint8_t, 7> L = {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
    static constexpr std::array<std::uint8_t, 7> M = {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
    static constexpr std::array<std::uint8_t, 7> N = {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001};
    static constexpr std::array<std::uint8_t, 7> O = {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> P = {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
    static constexpr std::array<std::uint8_t, 7> Q = {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
    static constexpr std::array<std::uint8_t, 7> R = {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
    static constexpr std::array<std::uint8_t, 7> S = {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
    static constexpr std::array<std::uint8_t, 7> T = {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    static constexpr std::array<std::uint8_t, 7> U = {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> V = {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100};
    static constexpr std::array<std::uint8_t, 7> W = {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001};
    static constexpr std::array<std::uint8_t, 7> X = {0b10001, 0b01010, 0b00100, 0b00100, 0b01010, 0b10001, 0b10001};
    static constexpr std::array<std::uint8_t, 7> Y = {0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    static constexpr std::array<std::uint8_t, 7> Z = {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111};
    static constexpr std::array<std::uint8_t, 7> N0 = {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> N1 = {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    static constexpr std::array<std::uint8_t, 7> N2 = {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
    static constexpr std::array<std::uint8_t, 7> N3 = {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
    static constexpr std::array<std::uint8_t, 7> N4 = {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
    static constexpr std::array<std::uint8_t, 7> N5 = {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110};
    static constexpr std::array<std::uint8_t, 7> N6 = {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> N7 = {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
    static constexpr std::array<std::uint8_t, 7> N8 = {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> N9 = {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};
    static constexpr std::array<std::uint8_t, 7> COLON = {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000};
    static constexpr std::array<std::uint8_t, 7> LT = {0b00001, 0b00010, 0b00100, 0b01000, 0b00100, 0b00010, 0b00001};
    static constexpr std::array<std::uint8_t, 7> GT = {0b10000, 0b01000, 0b00100, 0b00010, 0b00100, 0b01000, 0b10000};
    static constexpr std::array<std::uint8_t, 7> PLUS = {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000};
    static constexpr std::array<std::uint8_t, 7> MINUS = {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000};
    static constexpr std::array<std::uint8_t, 7> EQUAL = {0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000};
    static constexpr std::array<std::uint8_t, 7> DOT = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00110};
    static constexpr std::array<std::uint8_t, 7> UNDERSCORE = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111};
    switch (c) {
        case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D; case 'E': return E;
        case 'F': return F; case 'G': return G; case 'H': return H; case 'I': return I; case 'J': return J;
        case 'K': return K; case 'L': return L; case 'M': return M; case 'N': return N; case 'O': return O;
        case 'P': return P; case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
        case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X; case 'Y': return Y;
        case 'Z': return Z; case '0': return N0; case '1': return N1; case '2': return N2; case '3': return N3;
        case '4': return N4; case '5': return N5; case '6': return N6; case '7': return N7; case '8': return N8;
        case '9': return N9; case ':': return COLON; case '<': return LT; case '>': return GT; case '+': return PLUS;
        case '-': return MINUS; case '=': return EQUAL; case '.': return DOT; case '_': return UNDERSCORE; default: return SP;
    }
}

inline void pushQuadPx(std::vector<float>& v, const float x0, const float y0, const float x1, const float y1) {
    v.insert(v.end(), {x0,y0,x1,y0,x1,y1,x0,y0,x1,y1,x0,y1});
}
inline void pushLinePx(std::vector<float>& v, const float x0, const float y0, const float x1, const float y1) {
    v.insert(v.end(), {x0,y0,x1,y1});
}
inline void pushThickLinePx(
    std::vector<float>& v,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float thicknessPx)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float lenSq = dx * dx + dy * dy;
    if (lenSq <= 1e-6f) {
        const float half = std::max(0.5f, thicknessPx * 0.5f);
        pushQuadPx(v, x0 - half, y0 - half, x0 + half, y0 + half);
        return;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const float nx = -dy * invLen;
    const float ny = dx * invLen;
    const float half = std::max(0.5f, thicknessPx * 0.5f);
    const float ox = nx * half;
    const float oy = ny * half;
    v.insert(v.end(), {
        x0 - ox, y0 - oy,
        x1 - ox, y1 - oy,
        x1 + ox, y1 + oy,
        x0 - ox, y0 - oy,
        x1 + ox, y1 + oy,
        x0 + ox, y0 + oy});
}
inline void pushFramePx(std::vector<float>& v, const float x0, const float y0, const float x1, const float y1, const float t) {
    pushQuadPx(v,x0,y0,x1,y0+t);
    pushQuadPx(v,x0,y1-t,x1,y1);
    pushQuadPx(v,x0,y0+t,x0+t,y1-t);
    pushQuadPx(v,x1-t,y0+t,x1,y1-t);
}
inline float measureLinePx(const std::string& s, const float scalePx) {
    int count = 0;
    for (const char c : s) {
        if (c == '\n') break;
        (void)c;
        count += kAdvance;
    }
    return static_cast<float>(count) * scalePx;
}
inline float measureMaxLinePx(const std::string& s, const float scalePx) {
    int lineChars = 0;
    int maxChars = 0;
    for (const char c : s) {
        if (c == '\n') {
            maxChars = std::max(maxChars, lineChars);
            lineChars = 0;
            continue;
        }
        (void)c;
        lineChars += kAdvance;
    }
    maxChars = std::max(maxChars, lineChars);
    return static_cast<float>(maxChars) * scalePx;
}
inline float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
    const float unitWidth = measureMaxLinePx(s, 1.0f);
    if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) return preferredScalePx;
    return std::min(preferredScalePx, maxWidthPx / unitWidth);
}
inline void formatHudSpeed(char* out, const std::size_t outSize, const double simSpeed) {
    const double absSpeed = std::abs(simSpeed);
    if (absSpeed < 0.01) {
        std::snprintf(out, outSize, "SPEED: %.4fX", simSpeed);
        return;
    }
    if (absSpeed < 0.1) {
        std::snprintf(out, outSize, "SPEED: %.3fX", simSpeed);
        return;
    }
    std::snprintf(out, outSize, "SPEED: %.2fX", simSpeed);
}

inline void formatElapsedTime(char* out, const std::size_t outSize, const double elapsedSeconds) {
    const long long totalSeconds = static_cast<long long>(std::max(0.0, std::floor(elapsedSeconds)));
    const long long hours = totalSeconds / 3600;
    const long long minutes = (totalSeconds / 60) % 60;
    const long long seconds = totalSeconds % 60;
    std::snprintf(out, outSize, "TIME: %02lld:%02lld:%02lld", hours, minutes, seconds);
}

inline void appendTextPx(std::vector<float>& out, const float x, const float y, const float scalePx, const std::string& s) {
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
                if (((bits >> (kFontW - 1 - col)) & 1U) == 0U) continue;
                const float x0 = penX + static_cast<float>(col) * cell;
                const float y0 = penY + static_cast<float>(row) * cell;
                pushQuadPx(out, x0, y0, x0 + cell, y0 + cell);
            }
        }
        penX += static_cast<float>(kAdvance) * cell;
    }
}

struct ButtonPalette {
    std::vector<float>& fill;
    std::vector<float>& frame;
    std::vector<float>& text;
};
struct ButtonStyle {
    ButtonPalette normal;
    ButtonPalette hover;
    ButtonPalette selected;
};

inline void drawButtonPx(const ButtonStyle& style, const float x0, const float y0, const float x1, const float y1, const float padX,
    const float padY, const float textScalePx, const std::string& label, const bool hovered, const bool selected, const float frameThickness = 1.5f) {
    const ButtonPalette& palette = selected ? style.selected : (hovered ? style.hover : style.normal);
    pushQuadPx(palette.fill, x0, y0, x1, y1);
    pushFramePx(palette.frame, x0, y0, x1, y1, frameThickness);
    appendTextPx(palette.text, x0 + padX, y0 + padY, textScalePx, label);
}

inline void drawButtonPx(const ButtonStyle& style, const ui::pause_menu_layout::Rect& rect, const float padX, const float padY,
    const float textScalePx, const std::string& label, const bool hovered, const bool selected, const float frameThickness = 1.5f) {
    drawButtonPx(style, rect.x0, rect.y0, rect.x1, rect.y1, padX, padY, textScalePx, label, hovered, selected, frameThickness);
}

} // namespace overlay_renderer::draw_shared

#endif // PHYSICS3D_UI_OVERLAYRENDERERDRAWSHARED_H


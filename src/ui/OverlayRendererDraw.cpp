#include "ui/OverlayRendererInternal.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace overlay_renderer {

// ---------------- Tiny 5x7 font ----------------
static constexpr int kFontW = 5;
static constexpr int kFontH = 7;
static constexpr int kAdvance = 6; // 5 pixels + 1 spacing
static constexpr float kBaseScalePx = 2.0f;
static constexpr float kSettingsScalePx = 2.10f;
static constexpr float kTitleScalePx = 3.0f;

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
    static constexpr std::array<std::uint8_t, 7> LT    = {0b00001,0b00010,0b00100,0b01000,0b00100,0b00010,0b00001};
    static constexpr std::array<std::uint8_t, 7> GT    = {0b10000,0b01000,0b00100,0b00010,0b00100,0b01000,0b10000};
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
        case '<': return LT;
        case '>': return GT;
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

static float measureMaxLinePx(const std::string& s, const float scalePx) {
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

static float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
    const float unitWidth = measureMaxLinePx(s, 1.0f);
    if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) {
        return preferredScalePx;
    }
    return std::min(preferredScalePx, maxWidthPx / unitWidth);
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

void drawPauseMenu(
    const OverlayRenderer::PauseMenuHud& pauseMenu,
    const Geometry& geometry,
    const Buffers& buffers)
{
    const float w = geometry.width;
    const float h = geometry.height;
    const float menuScale = std::clamp(geometry.uiScale, 0.80f, 1.40f);
    const float baseScalePx = kBaseScalePx * menuScale;
    const float preferredRowScalePx = kSettingsScalePx * menuScale;
    const float preferredTitleScalePx = kTitleScalePx * menuScale;

    pushQuadPx(buffers.screenDim, 0.0f, 0.0f, w, h);

    const float cardW = std::min(w * 0.82f, 1240.0f);
    const float cardH = std::min(h * 0.86f, 920.0f);
    const float cardX0 = (w - cardW) * 0.5f;
    const float cardY0 = (h - cardH) * 0.5f;
    const float cardX1 = cardX0 + cardW;
    const float cardY1 = cardY0 + cardH;

    pushQuadPx(buffers.panelFill, cardX0, cardY0, cardX1, cardY1);
    pushFramePx(buffers.panelFrame, cardX0, cardY0, cardX1, cardY1, 2.0f);
    pushQuadPx(buffers.accentFill, cardX0, cardY0, cardX1, cardY0 + 5.0f);

    const float centerX = w * 0.5f;
    const float infoWidth = cardW - 72.0f;

    const std::string title = "PAUSED";
    const std::string resume = "ESC: RESUME";
    const float titleScalePx = fitScaleForWidth(title, preferredTitleScalePx, infoWidth);
    const float resumeScalePx = fitScaleForWidth(resume, baseScalePx * 1.05f, infoWidth);

    appendTextPx(
        buffers.textAccent,
        centerX - measureMaxLinePx(title, titleScalePx) * 0.5f,
        cardY0 + 18.0f * menuScale,
        titleScalePx,
        title);
    appendTextPx(
        buffers.textMuted,
        centerX - measureMaxLinePx(resume, resumeScalePx) * 0.5f,
        cardY0 + 64.0f * menuScale,
        resumeScalePx,
        resume);

    const float statusSlotY = cardY0 + 96.0f * menuScale;
    const float statusReservedScalePx = baseScalePx * 0.95f;
    const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
    if (!pauseMenu.statusLine.empty()) {
        const float statusScalePx = fitScaleForWidth(pauseMenu.statusLine, baseScalePx * 0.95f, infoWidth);
        appendTextPx(
            pauseMenu.awaitingBind ? buffers.textWarning : buffers.textAccent,
            centerX - measureMaxLinePx(pauseMenu.statusLine, statusScalePx) * 0.5f,
            statusSlotY,
            statusScalePx,
            pauseMenu.statusLine);
    }

    const float tabsSlotY = statusSlotY + statusReservedHeight + 16.0f * menuScale;
    constexpr std::array<const char*, 5> kPageTabLabels{{"DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS"}};
    const float tabsScalePx = baseScalePx * 1.00f;
    const float tabPadX = 12.0f * menuScale;
    const float tabPadY = 7.0f * menuScale;
    const float tabGap = 12.0f * menuScale;
    std::array<float, 5> tabWidths{};
    for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
        tabWidths[i] = measureMaxLinePx(kPageTabLabels[i], tabsScalePx) + tabPadX * 2.0f;
    }
    const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * tabsScalePx + tabPadY * 2.0f;
    const float buttonPadX = 12.0f * menuScale;
    const std::string exitLabel = "EXIT TO HOME";
    const float exitWidth = measureMaxLinePx(exitLabel, tabsScalePx) + buttonPadX * 2.0f;
    const float exitX1 = cardX1 - 18.0f * menuScale;
    const float exitX0 = exitX1 - exitWidth;
    const float exitY0 = tabsSlotY;
    const float exitY1 = exitY0 + tabHeight;
    const std::string closeLabel = "X";
    const float closePadX = 12.0f * menuScale;
    const float closeWidth = measureMaxLinePx(closeLabel, tabsScalePx) + closePadX * 2.0f;
    const float closeX1 = cardX1 - 18.0f * menuScale;
    const float closeX0 = closeX1 - closeWidth;
    const float closeY0 = cardY0 + 18.0f * menuScale;
    const float closeY1 = closeY0 + tabHeight;
    const std::string resetIconLabel = "R";
    const float resetPadX = 12.0f * menuScale;
    const float resetWidth = measureMaxLinePx(resetIconLabel, tabsScalePx) + resetPadX * 2.0f;
    const float resetX0 = cardX0 + 18.0f * menuScale;
    const float resetX1 = resetX0 + resetWidth;
    const float resetY0 = cardY0 + 18.0f * menuScale;
    const float resetY1 = resetY0 + tabHeight;

    const float tabXStart = cardX0 + 18.0f * menuScale;
    float tabX = tabXStart;
    const int activeTab = std::clamp(pauseMenu.activePageIndex, 0, static_cast<int>(kPageTabLabels.size()) - 1);
    for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
        const float x0 = tabX;
        const float x1 = x0 + tabWidths[i];
        const float y0 = tabsSlotY;
        const float y1 = y0 + tabHeight;
        const bool active = static_cast<int>(i) == activeTab;
        if (active) {
            pushQuadPx(buffers.accentFill, x0, y0, x1, y1);
        }
        pushFramePx(buffers.panelFrame, x0, y0, x1, y1, 1.5f);
        appendTextPx(
            active ? buffers.textPrimary : buffers.textMuted,
            x0 + tabPadX,
            y0 + tabPadY,
            tabsScalePx,
            kPageTabLabels[i]);
        tabX = x1 + tabGap;
    }

    pushQuadPx(buffers.panelFill, exitX0, exitY0, exitX1, exitY1);
    pushFramePx(buffers.textWarning, exitX0, exitY0, exitX1, exitY1, 1.5f);
    appendTextPx(
        buffers.textWarning,
        exitX0 + buttonPadX,
        exitY0 + tabPadY,
        tabsScalePx,
        exitLabel);
    pushQuadPx(buffers.panelFill, closeX0, closeY0, closeX1, closeY1);
    pushFramePx(buffers.panelFrame, closeX0, closeY0, closeX1, closeY1, 1.5f);
    appendTextPx(
        buffers.textPrimary,
        closeX0 + closePadX,
        closeY0 + tabPadY,
        tabsScalePx,
        closeLabel);
    if (pauseMenu.showResetIcon) {
        pushQuadPx(buffers.panelFill, resetX0, resetY0, resetX1, resetY1);
        pushFramePx(
            pauseMenu.hoverResetIcon ? buffers.textWarning : buffers.panelFrame,
            resetX0,
            resetY0,
            resetX1,
            resetY1,
            1.5f);
        appendTextPx(
            pauseMenu.hoverResetIcon ? buffers.textWarning : buffers.textPrimary,
            resetX0 + resetPadX,
            resetY0 + tabPadY,
            tabsScalePx,
            resetIconLabel);
    }
    const float footerReservedH = (static_cast<float>(kFontH) + 3.0f) * (baseScalePx * 0.92f) + 18.0f * menuScale;
    float settingsY0 = std::max(cardY0 + 170.0f * menuScale, tabsSlotY + tabHeight + 16.0f * menuScale);

    const float sectionX0 = cardX0 + 26.0f * menuScale;
    const float sectionX1 = cardX1 - 26.0f * menuScale;
    const float settingsY1 = cardY1 - 20.0f * menuScale - footerReservedH;
    if (settingsY1 - settingsY0 < 220.0f) {
        settingsY0 = settingsY1 - 220.0f;
    }

    pushQuadPx(buffers.panelFill, sectionX0, settingsY0, sectionX1, settingsY1);
    pushFramePx(buffers.panelFrame, sectionX0, settingsY0, sectionX1, settingsY1, 1.5f);
    pushQuadPx(buffers.accentFill, sectionX0, settingsY0, sectionX1, settingsY0 + 3.0f);

    float rowScalePx = preferredRowScalePx * 1.8f;
    float maxRowWidth = 0.0f;
    float maxControlReserve = 0.0f;
    for (const auto& line : pauseMenu.lines) {
        maxRowWidth = std::max(maxRowWidth, measureMaxLinePx(line.text, 1.0f));
        if (line.header) {
            continue;
        }
        switch (line.controlType) {
            case OverlayRenderer::PauseMenuControlType::Toggle:
                maxControlReserve = std::max(maxControlReserve, 128.0f * menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Choice:
            case OverlayRenderer::PauseMenuControlType::Numeric:
                maxControlReserve = std::max(maxControlReserve, 248.0f * menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Rebind:
                maxControlReserve = std::max(maxControlReserve, 240.0f * menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Action:
                maxControlReserve = std::max(maxControlReserve, 120.0f * menuScale);
                break;
            default:
                break;
        }
    }
    const float rowAreaWidth = (sectionX1 - sectionX0) - 28.0f * menuScale - maxControlReserve;
    if (maxRowWidth > 0.0f && rowAreaWidth > 0.0f) {
        rowScalePx = std::min(rowScalePx, rowAreaWidth / maxRowWidth);
    }

    const std::string settingsTitle = "SETTINGS";
    const float sectionHeaderScalePx = fitScaleForWidth(settingsTitle, rowScalePx * 1.18f, (sectionX1 - sectionX0) - 28.0f);
    const float sectionHeaderY = settingsY0 + 10.0f * menuScale;
    appendTextPx(buffers.textAccent, sectionX0 + 14.0f * menuScale, sectionHeaderY, sectionHeaderScalePx, settingsTitle);

    const float linesStartY = sectionHeaderY + (static_cast<float>(kFontH) + 3.0f) * sectionHeaderScalePx + 9.0f * menuScale;
    const float rowStep = (static_cast<float>(kFontH) + 4.0f) * rowScalePx;
    const float settingsBottomPad = 10.0f * menuScale;
    const float actionReservedH =
        (pauseMenu.showDisplayApplyAction ||
         pauseMenu.showResetControlsAction) ? 44.0f * menuScale : 0.0f;
    const float contentY1 = settingsY1 - settingsBottomPad - actionReservedH;
    const float maxLines = std::floor((contentY1 - linesStartY) / rowStep);
    const std::size_t totalLines = pauseMenu.lines.size();
    const std::size_t visibleLines = std::min<std::size_t>(
        totalLines,
        static_cast<std::size_t>(std::max(0.0f, maxLines)));

    std::size_t firstLine = 0;
    if (visibleLines > 0 && totalLines > visibleLines) {
        const int clampedSelected = std::clamp(
            pauseMenu.selectedSettingLineIndex,
            0,
            static_cast<int>(totalLines - 1));
        const auto selected = static_cast<std::size_t>(clampedSelected);
        if (selected >= visibleLines) {
            firstLine = selected - visibleLines + 1;
        }
        const std::size_t maxFirst = totalLines - visibleLines;
        if (firstLine > maxFirst) {
            firstLine = maxFirst;
        }
    }

    if (visibleLines > 0 && totalLines > visibleLines) {
        const float trackW = 8.0f * menuScale;
        const float trackX1 = sectionX1 - 8.0f * menuScale;
        const float trackX0 = trackX1 - trackW;
        const float trackY0 = linesStartY;
        const float trackY1 = contentY1;
        pushQuadPx(buffers.panelFill, trackX0, trackY0, trackX1, trackY1);
        pushFramePx(buffers.panelFrame, trackX0, trackY0, trackX1, trackY1, 1.0f);

        const float visibleRatio = std::clamp(
            static_cast<float>(visibleLines) / static_cast<float>(totalLines),
            0.0f,
            1.0f);
        const float thumbH = std::max(18.0f * menuScale, (trackY1 - trackY0) * visibleRatio);
        const auto maxFirst = static_cast<float>(totalLines - visibleLines);
        const float scrollT = maxFirst > 0.0f
            ? static_cast<float>(firstLine) / maxFirst
            : 0.0f;
        const float thumbTravel = std::max(0.0f, (trackY1 - trackY0) - thumbH);
        const float thumbY0 = trackY0 + thumbTravel * scrollT;
        const float thumbY1 = thumbY0 + thumbH;
        pushQuadPx(buffers.accentFill, trackX0 + 1.0f, thumbY0 + 1.0f, trackX1 - 1.0f, thumbY1 - 1.0f);
    }

    if (pauseMenu.showDisplayApplyAction) {
        const std::string applyLabel = "APPLY CHANGES";
        const float actionScalePx = baseScalePx * 0.98f;
        const float actionPadX = 16.0f * menuScale;
        const float actionPadY = 8.0f * menuScale;
        const float actionW = measureMaxLinePx(applyLabel, actionScalePx) + actionPadX * 2.0f;
        const float actionH = (static_cast<float>(kFontH) + 2.0f) * actionScalePx + actionPadY * 2.0f;
        const float actionX1 = sectionX1 - 12.0f * menuScale;
        const float actionX0 = actionX1 - actionW;
        const float actionY1 = settingsY1 - 10.0f * menuScale;
        const float actionY0 = actionY1 - actionH;

        if (pauseMenu.hoverDisplayApplyAction) {
            pushQuadPx(buffers.panelFill, actionX0, actionY0, actionX1, actionY1);
        }
        pushFramePx(buffers.textAccent, actionX0, actionY0, actionX1, actionY1, 1.5f);
        appendTextPx(
            buffers.textAccent,
            actionX0 + actionPadX,
            actionY0 + actionPadY,
            actionScalePx,
            applyLabel);
    }
    if (pauseMenu.showResetControlsAction) {
        const std::string resetLabel = "RESET CONTROLS";
        const float actionScalePx = baseScalePx * 0.98f;
        const float actionPadX = 16.0f * menuScale;
        const float actionPadY = 8.0f * menuScale;
        const float actionW = measureMaxLinePx(resetLabel, actionScalePx) + actionPadX * 2.0f;
        const float actionH = (static_cast<float>(kFontH) + 2.0f) * actionScalePx + actionPadY * 2.0f;
        const float actionX1 = sectionX1 - 12.0f * menuScale;
        const float actionX0 = actionX1 - actionW;
        const float actionY1 = settingsY1 - 10.0f * menuScale;
        const float actionY0 = actionY1 - actionH;

        if (pauseMenu.hoverResetControlsAction) {
            pushQuadPx(buffers.panelFill, actionX0, actionY0, actionX1, actionY1);
        }
        pushFramePx(buffers.textWarning, actionX0, actionY0, actionX1, actionY1, 1.5f);
        appendTextPx(
            buffers.textWarning,
            actionX0 + actionPadX,
            actionY0 + actionPadY,
            actionScalePx,
            resetLabel);
    }
    if (pauseMenu.showResetConfirm) {
        const float popupW = 320.0f * menuScale;
        const float popupH = 120.0f * menuScale;
        const float popupX0 = cardX0 + (cardW - popupW) * 0.5f;
        const float popupY0 = cardY0 + (cardH - popupH) * 0.5f;
        const float popupX1 = popupX0 + popupW;
        const float popupY1 = popupY0 + popupH;
        const float popupScalePx = baseScalePx * 0.98f;
        const float buttonScalePx = baseScalePx * 0.92f;
        const float buttonPadX1 = 16.0f * menuScale;
        const float buttonPadY = 8.0f * menuScale;
        const std::string titleText = "ARE YOU SURE?";
        const std::string bodyText = "RESET ALL SETTINGS";
        const std::string yesLabel = "RESET";
        const std::string noLabel = "CANCEL";
        const float yesW = measureMaxLinePx(yesLabel, buttonScalePx) + buttonPadX1 * 2.0f;
        const float noW = measureMaxLinePx(noLabel, buttonScalePx) + buttonPadX1 * 2.0f;
        const float buttonH = (static_cast<float>(kFontH) + 2.0f) * buttonScalePx + buttonPadY * 2.0f;
        const float buttonsY1 = popupY1 - 14.0f * menuScale;
        const float buttonsY0 = buttonsY1 - buttonH;
        const float noX1 = popupX1 - 18.0f * menuScale;
        const float noX0 = noX1 - noW;
        const float yesX1 = noX0 - 10.0f * menuScale;
        const float yesX0 = yesX1 - yesW;

        pushQuadPx(buffers.popupBg, popupX0, popupY0, popupX1, popupY1);
        pushFramePx(buffers.popupFrame, popupX0, popupY0, popupX1, popupY1, 1.5f);
        pushQuadPx(buffers.textWarning, popupX0, popupY0, popupX1, popupY0 + 3.0f);
        appendTextPx(
            buffers.popupText,
            popupX0 + 16.0f * menuScale,
            popupY0 + 14.0f * menuScale,
            popupScalePx,
            titleText);
        appendTextPx(
            buffers.popupText,
            popupX0 + 16.0f * menuScale,
            popupY0 + 36.0f * menuScale,
            buttonScalePx,
            bodyText);

        if (pauseMenu.hoverResetConfirmYes) {
            pushQuadPx(buffers.panelFill, yesX0, buttonsY0, yesX1, buttonsY1);
        }
        pushFramePx(buffers.textWarning, yesX0, buttonsY0, yesX1, buttonsY1, 1.5f);
        appendTextPx(buffers.textWarning, yesX0 + buttonPadX1, buttonsY0 + buttonPadY, buttonScalePx, yesLabel);

        if (pauseMenu.hoverResetConfirmNo) {
            pushQuadPx(buffers.panelFill, noX0, buttonsY0, noX1, buttonsY1);
        }
        pushFramePx(buffers.panelFrame, noX0, buttonsY0, noX1, buttonsY1, 1.5f);
        appendTextPx(buffers.textPrimary, noX0 + buttonPadX1, buttonsY0 + buttonPadY, buttonScalePx, noLabel);
    }

    for (std::size_t i = 0; i < visibleLines; ++i) {
        const std::size_t lineIdx = firstLine + i;
        const bool selected = static_cast<int>(lineIdx) == pauseMenu.selectedSettingLineIndex;
        const bool hovered = static_cast<int>(lineIdx) == pauseMenu.hoveredSettingLineIndex;
        const bool header = pauseMenu.lines[lineIdx].header;
        const bool disabled = pauseMenu.lines[lineIdx].disabled;
        const float lineY = linesStartY + static_cast<float>(i) * rowStep;
        const float lineX0 = sectionX0 + 10.0f * menuScale;
        const float lineX1 = sectionX1 - 10.0f * menuScale;

        if (selected && !header) {
            pushFramePx(
                buffers.textMuted,
                lineX0,
                lineY - 2.0f * menuScale,
                lineX1,
                lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale,
                1.5f);
        } else if (hovered && !header) {
            pushFramePx(
                buffers.panelFrame,
                lineX0,
                lineY - 2.0f * menuScale,
                lineX1,
                lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale,
                1.0f);
        }
        const float lineScalePx = header
            ? fitScaleForWidth(pauseMenu.lines[lineIdx].text, rowScalePx * 1.03f, rowAreaWidth + maxControlReserve)
            : rowScalePx;
        appendTextPx(
            header ? buffers.textAccent : (disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted)),
            sectionX0 + 14.0f * menuScale,
            lineY,
            lineScalePx,
            pauseMenu.lines[lineIdx].text);

        if (header) {
            continue;
        }

        const std::string valueText = pauseMenu.lines[lineIdx].valueText;
        const float valueScalePx = rowScalePx * 0.88f;
        const float buttonY0 = lineY - 2.0f * menuScale;
        const float buttonY1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale;
        const auto drawButton = [&](const float x0, const float x1, const std::string& text, const bool active, const bool warning) {
            if (active && !disabled && !selected) {
                pushQuadPx(warning ? buffers.textWarning : buffers.accentFill, x0, buttonY0, x1, buttonY1);
            }
            if (active) {
                pushFramePx(
                    disabled ? buffers.panelFrame : (buffers.textPrimary),
                    x0,
                    buttonY0,
                    x1,
                    buttonY1,
                    1.0f);
            }
            const float tx = x0 + (x1 - x0 - measureMaxLinePx(text, valueScalePx)) * 0.5f;
            appendTextPx(
                disabled ? buffers.textMuted
                         : (active ? buffers.textPrimary : (warning ? buffers.textWarning : buffers.textMuted)),
                tx,
                lineY + 0.3f * menuScale,
                valueScalePx,
                text);
        };

        switch (pauseMenu.lines[lineIdx].controlType) {
            case OverlayRenderer::PauseMenuControlType::Toggle: {
                const float valueW = 220.0f * menuScale;
                const float rightX1 = lineX1 - 10.0f * menuScale;
                const float valueX1 = rightX1 - 24.0f * menuScale - 6.0f * menuScale;
                const float x1 = valueX1;
                const float x0 = x1 - valueW;
                const float box = std::max(8.0f, (buttonY1 - buttonY0) - 8.0f * menuScale);
                const std::string toggleText = pauseMenu.lines[lineIdx].boolValue ? "ON" : "OFF";
                const float gap = 5.0f * menuScale;
                const float bx0 = x0 + 18.0f * menuScale;
                const float by0 = buttonY0 + (buttonY1 - buttonY0 - box) * 0.5f;
                const float textX0 = bx0 + box + gap;
                const float textW = measureMaxLinePx(toggleText, valueScalePx);
                const float textX = textX0 + std::max(0.0f, ((x1 - 12.0f * menuScale) - textX0 - textW) * 0.5f);
                drawButton(x0, x1, "", false, false);
                pushFramePx(buffers.textMuted, bx0, by0, bx0 + box, by0 + box, 1.1f);
                if (pauseMenu.lines[lineIdx].boolValue) {
                    pushQuadPx(
                        disabled ? buffers.panelFrame : buffers.accentFill,
                        bx0 + 2.0f,
                        by0 + 2.0f,
                        bx0 + box - 2.0f,
                        by0 + box - 2.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted),
                    textX,
                    lineY + 3.3f * menuScale,
                    valueScalePx,
                    toggleText);
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Choice:
            case OverlayRenderer::PauseMenuControlType::Numeric: {
                const float arrowW = 24.0f * menuScale;
                const float gap = 6.0f * menuScale;
                const float valueW = 220.0f * menuScale;
                const bool numeric = pauseMenu.lines[lineIdx].controlType == OverlayRenderer::PauseMenuControlType::Numeric;
                const char* leftSymbol = numeric ? "-" : "<";
                const char* rightSymbol = numeric ? "+" : ">";
                const float rightX1 = lineX1 - 10.0f * menuScale;
                const float rightArrowX0 = rightX1 - arrowW;
                const float valueX1 = rightArrowX0 - gap;
                const float valueX0 = valueX1 - valueW;
                const float leftArrowX1 = valueX0 - gap;
                const float leftArrowX0 = leftArrowX1 - arrowW;

                if (disabled) {
                    appendTextPx(
                        buffers.textMuted,
                        valueX0 + (valueW - measureMaxLinePx(valueText, valueScalePx)) * 0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        valueText);
                } else {
                    drawButton(leftArrowX0, leftArrowX1, leftSymbol, false, false);
                    drawButton(rightArrowX0, rightX1, rightSymbol, false, false);
                    drawButton(valueX0, valueX1, valueText, false, false);
                }
                if (!disabled && numeric && pauseMenu.lines[lineIdx].showSlider)
                {
                    const float sliderX0 = valueX0 + 6.0f * menuScale;
                    const float sliderX1 = valueX1 - 6.0f * menuScale;
                    const float sliderY1 = buttonY1 - 3.0f * menuScale;
                    const float sliderY0 = sliderY1 - 6.0f * menuScale;
                    pushQuadPx(buffers.panelFrame, sliderX0, sliderY0, sliderX1, sliderY1);
                    const float fillX = sliderX0 +
                        std::clamp(pauseMenu.lines[lineIdx].sliderT, 0.0f, 1.0f) * (sliderX1 - sliderX0);
                    pushQuadPx(buffers.accentFill, sliderX0, sliderY0, fillX, sliderY1);
                }
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Rebind: {
                const float valueW = std::clamp(
                    measureMaxLinePx(valueText, valueScalePx) + 18.0f * menuScale,
                    88.0f * menuScale,
                    150.0f * menuScale);
                const float valueX1 = lineX1 - 10.0f * menuScale;
                const float valueX0 = valueX1 - valueW;
                appendTextPx(
                    buffers.textMuted,
                    valueX0 + (valueW - measureMaxLinePx(valueText, valueScalePx)) * 0.5f,
                    lineY + 0.3f * menuScale,
                    valueScalePx,
                    valueText);
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Action: {
                if (!valueText.empty()) {
                    const float valueW = std::clamp(
                        measureMaxLinePx(valueText, valueScalePx) + 18.0f * menuScale,
                        88.0f * menuScale,
                        150.0f * menuScale);
                    const float valueX1 = lineX1 - 10.0f * menuScale;
                    const float valueX0 = valueX1 - valueW;
                    appendTextPx(
                        selected ? buffers.textPrimary : buffers.textMuted,
                        valueX0 + (valueW - measureMaxLinePx(valueText, valueScalePx)) * 0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        valueText);
                }
                break;
            }
            case OverlayRenderer::PauseMenuControlType::None:
            default:
                if (!valueText.empty()) {
                    const float valueW = measureMaxLinePx(valueText, valueScalePx);
                    appendTextPx(
                        buffers.textMuted,
                        lineX1 - 12.0f * menuScale - valueW,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        valueText);
                }
                break;
        }
    }

    if (!pauseMenu.footerHint.empty()) {
        const float footerX0 = sectionX0;
        const float footerX1 = sectionX1;
        const float footerY0 = settingsY1 + 10.0f * menuScale;
        const float footerY1 = cardY1 - 16.0f * menuScale;
        pushQuadPx(buffers.panelFill, footerX0, footerY0, footerX1, footerY1);
        pushFramePx(buffers.panelFrame, footerX0, footerY0, footerX1, footerY1, 1.2f);
        const float footerScalePx = fitScaleForWidth(
            pauseMenu.footerHint,
            baseScalePx * 0.92f,
            (footerX1 - footerX0) - 18.0f * menuScale);
        appendTextPx(
            buffers.textMuted,
            footerX0 + 10.0f * menuScale,
            footerY0 + 6.0f * menuScale,
            footerScalePx,
            pauseMenu.footerHint);
    }
}

void drawHud(
    const Geometry& geometry,
    const bool simFrozen,
    const double simSpeed,
    const double fps,
    const std::vector<std::string>& hudDebugLines,
    const Buffers& buffers)
{
    const float baseScalePx = kBaseScalePx * geometry.uiScale;
    constexpr float hudX0 = 16.0f;
    constexpr float hudY0 = 16.0f;
    constexpr float hudX1 = hudX0 + 252.0f;
    constexpr float hudTextX = hudX0 + 10.0f;
    constexpr float hudTextY0 = hudY0 + 10.0f;
    const float hudRowStep = 20.0f * geometry.uiScale;
    const float hudY1 = hudY0 + 16.0f + (4.0f + static_cast<float>(hudDebugLines.size())) * hudRowStep;
    char speedLine[64];
    std::snprintf(speedLine, sizeof(speedLine), "SPEED: %.2fX", simSpeed);
    char fpsLine[64];
    std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);
    pushQuadPx(buffers.panelFill, hudX0, hudY0, hudX1, hudY1);
    pushFramePx(buffers.panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);
    pushQuadPx(buffers.accentFill, hudX0, hudY0, hudX1, hudY0 + 3.0f);

    appendTextPx(
        buffers.statusText,
        hudTextX,
        hudTextY0,
        baseScalePx,
        simFrozen ? "WORLD: FROZEN" : "WORLD: RUNNING");
    appendTextPx(buffers.textMuted, hudTextX, hudTextY0 + hudRowStep, baseScalePx, "ESC: MENU");
    appendTextPx(buffers.textPrimary, hudTextX, hudTextY0 + hudRowStep * 2.0f, baseScalePx, speedLine);
    appendTextPx(buffers.textPrimary, hudTextX, hudTextY0 + hudRowStep * 3.0f, baseScalePx, fpsLine);
    for (std::size_t i = 0; i < hudDebugLines.size(); ++i) {
        appendTextPx(
            buffers.textMuted,
            hudTextX,
            hudTextY0 + hudRowStep * (4.0f + static_cast<float>(i)),
            baseScalePx,
            hudDebugLines[i]);
    }
}

void drawCrosshair(const Geometry& geometry, const Buffers& buffers) {
    const float cx = geometry.width * 0.5f;
    const float cy = geometry.height * 0.5f;
    constexpr float gap = 5.0f;
    constexpr float len = 8.0f;
    constexpr float thick = 1.0f;
    pushQuadPx(buffers.crosshair, cx - len - gap, cy - thick, cx - gap, cy + thick);
    pushQuadPx(buffers.crosshair, cx + gap, cy - thick, cx + len + gap, cy + thick);
    pushQuadPx(buffers.crosshair, cx - thick, cy - len - gap, cx + thick, cy - gap);
    pushQuadPx(buffers.crosshair, cx - thick, cy + gap, cx + thick, cy + len + gap);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
}

void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers)
{
    if (!targetHud.visible || targetHud.lines.empty()) {
        return;
    }
    const float baseScalePx = kBaseScalePx * geometry.uiScale;

    float maxLineW = 0.0f;
    std::string popup;
    popup.reserve(192);
    for (std::size_t i = 0; i < targetHud.lines.size(); ++i) {
        if (i > 0) {
            popup += '\n';
        }
        popup += targetHud.lines[i];
        maxLineW = std::max(maxLineW, measureLinePx(targetHud.lines[i], baseScalePx));
    }

    const float popupW = maxLineW + 24.0f;
    const float popupH = static_cast<float>((kFontH + 2) * static_cast<int>(targetHud.lines.size())) * baseScalePx + 12.0f;
    const float px = targetHud.xPx - popupW * 0.5f;
    const float py = targetHud.yPx - popupH - 12.0f;

    pushQuadPx(buffers.popupBg, px, py, px + popupW, py + popupH);
    pushFramePx(buffers.popupFrame, px, py, px + popupW, py + popupH, 1.5f);
    pushQuadPx(buffers.accentFill, px, py, px + popupW, py + 3.0f);
    appendTextPx(buffers.popupText, px + 8.0f, py + 6.0f, baseScalePx, popup);
}


} // namespace overlay_renderer

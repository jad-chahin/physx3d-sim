#include "ui/OverlayRenderer.h"
#include "ui/OverlayRendererDrawShared.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {

GLuint compileShader(const GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint linkProgram(const GLuint vs, const GLuint fs) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
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
void main() { FragColor = uColor; }
)GLSL";

void drawLayer(
    const GLint uColor,
    const std::vector<float>& vertices,
    const std::array<float, 4>& color)
{
    if (vertices.empty()) {
        return;
    }
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW);
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 2));
}

void drawLineLayer(
    const GLint uColor,
    const std::vector<float>& vertices,
    const std::array<float, 4>& color)
{
    if (vertices.empty()) {
        return;
    }
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW);
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 2));
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
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    vbo_ = 0;
    vao_ = 0;
    program_ = 0;
}

void OverlayRenderer::draw(
    int fbw,
    int fbh,
    bool simFrozen,
    double simSpeed,
    double fps,
    const PauseMenuHud& pauseMenu,
    const TargetHud& targetHud,
    const std::vector<ScreenLine>& pathLines,
    float uiScale,
    bool showHud,
    bool showCrosshair,
    const std::vector<std::string>& hudDebugLines) const
{
    std::vector<float> screenDim, panelFill, panelFrame, accentFill, textPrimary, textMuted,
        textAccent, textWarning, statusText, crosshair, screenPathLines, popupBg, popupFrame,
        popupAccent, popupText;
    screenDim.reserve(24);
    panelFill.reserve(600);
    panelFrame.reserve(600);
    accentFill.reserve(600);
    textPrimary.reserve(9000);
    textMuted.reserve(9000);
    textAccent.reserve(9000);
    textWarning.reserve(3000);
    statusText.reserve(2000);
    crosshair.reserve(24);
    screenPathLines.reserve(pathLines.size() * 4);
    popupText.reserve(1200);

    const overlay_renderer::Geometry geometry{
        static_cast<float>(fbw),
        static_cast<float>(fbh),
        std::clamp(uiScale, 0.75f, 2.0f),
    };
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
        screenPathLines,
        popupBg,
        popupFrame,
        popupAccent,
        popupText,
    };
    const bool frozenOverlay = simFrozen && !pauseMenu.visible;

    if (pauseMenu.visible) {
        overlay_renderer::drawPauseMenu(pauseMenu, geometry, buffers);
    } else if (showHud) {
        overlay_renderer::drawHud(geometry, simFrozen, simSpeed, fps, hudDebugLines, buffers);
    }
    if (!pauseMenu.visible && showCrosshair) {
        overlay_renderer::drawCrosshair(geometry, simFrozen, buffers);
    }
    if (!pauseMenu.visible && !pathLines.empty()) {
        overlay_renderer::drawPathLines(geometry, pathLines, buffers);
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

    drawLayer(
        uColor_,
        screenDim,
        frozenOverlay
            ? std::array<float, 4>{0.05f, 0.09f, 0.12f, 0.68f}
            : std::array<float, 4>{0.01f, 0.03f, 0.05f, pauseMenu.visible ? 0.42f : 0.62f});
    drawLayer(
        uColor_,
        panelFill,
        {pauseMenu.visible ? 0.03f : 0.05f,
         pauseMenu.visible ? 0.05f : 0.07f,
         pauseMenu.visible ? 0.07f : 0.10f,
         pauseMenu.visible ? 0.56f : 0.90f});
    drawLayer(uColor_, panelFrame, {0.18f, 0.24f, 0.30f, pauseMenu.visible ? 0.78f : 0.98f});
    drawLayer(
        uColor_,
        accentFill,
        frozenOverlay
            ? std::array<float, 4>{0.58f, 0.83f, 0.92f, 0.88f}
            : std::array<float, 4>{0.23f, 0.70f, 0.92f, pauseMenu.visible ? 0.72f : 0.95f});
    drawLayer(uColor_, textPrimary, {1.0f, 1.0f, 1.0f, 1.0f});
    drawLayer(uColor_, textMuted, {0.74f, 0.80f, 0.86f, 0.98f});
    drawLayer(uColor_, textAccent, {0.37f, 0.81f, 0.97f, 1.0f});
    drawLayer(uColor_, textWarning, {1.0f, 0.62f, 0.31f, 1.0f});
    drawLayer(uColor_, popupText, {0.95f, 0.97f, 1.0f, 1.0f});
    drawLayer(
        uColor_,
        statusText,
        simFrozen
            ? std::array<float, 4>{0.74f, 0.91f, 1.0f, 1.0f}
            : std::array<float, 4>{0.48f, 0.88f, 0.54f, 1.0f});
    drawLineLayer(
        uColor_,
        screenPathLines,
        frozenOverlay
            ? std::array<float, 4>{0.70f, 0.90f, 1.0f, 0.32f}
            : std::array<float, 4>{0.37f, 0.81f, 0.97f, 0.45f});
    drawLayer(
        uColor_,
        crosshair,
        frozenOverlay
            ? std::array<float, 4>{0.74f, 0.91f, 1.0f, 1.0f}
            : std::array<float, 4>{0.32f, 0.85f, 0.95f, 1.0f});
    drawLayer(uColor_, popupBg, {0.05f, 0.07f, 0.10f, 1.0f});
    drawLayer(uColor_, popupFrame, {0.18f, 0.24f, 0.30f, 0.98f});
    drawLayer(uColor_, popupAccent, {1.0f, 0.62f, 0.31f, 1.0f});
    drawLayer(uColor_, popupText, {0.95f, 0.97f, 1.0f, 1.0f});

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

namespace overlay_renderer {
using namespace draw_shared;

namespace {

constexpr std::size_t actionIndex(const OverlayRenderer::PauseMenuAction action) {
    return static_cast<std::size_t>(action);
}

struct ActionButtonDef {
    OverlayRenderer::PauseMenuAction action;
    const char* label;
};

} // namespace

void drawPauseMenu(
    const OverlayRenderer::PauseMenuHud& pauseMenu,
    const Geometry& geometry,
    const Buffers& buffers)
{
    const float w = geometry.width;
    const float h = geometry.height;
    const ui::pause_menu_layout::PauseMenuLayout l =
        ui::pause_menu_layout::buildPauseMenuLayout(w, h, geometry.uiScale, pauseMenu);
    const float menuScale = l.menuScale;
    const float baseScalePx = l.baseScalePx;
    const float preferredTitleScalePx = kTitleScalePx * menuScale;

    pushQuadPx(buffers.screenDim, 0.0f, 0.0f, w, h);

    const float cardX0 = l.cardX0;
    const float cardY0 = l.cardY0;
    const float cardX1 = l.cardX1;
    const float cardY1 = l.cardY1;
    const float cardW = cardX1 - cardX0;
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

    if (!pauseMenu.statusLine.empty()) {
        const float statusScalePx =
            fitScaleForWidth(pauseMenu.statusLine, baseScalePx * 0.95f, infoWidth);
        appendTextPx(
            pauseMenu.awaitingBind ? buffers.textWarning : buffers.textAccent,
            centerX - measureMaxLinePx(pauseMenu.statusLine, statusScalePx) * 0.5f,
            cardY0 + 96.0f * menuScale,
            statusScalePx,
            pauseMenu.statusLine);
    }

    const float tabsScalePx = l.tabsScalePx;
    const float tabPadX = 12.0f * menuScale;
    const float tabPadY = 7.0f * menuScale;
    const ButtonStyle pauseButtonStyle{
        {buffers.panelFill, buffers.panelFrame, buffers.textMuted},
        {buffers.panelFrame, buffers.panelFrame, buffers.textPrimary},
        {buffers.panelFill, buffers.textPrimary, buffers.textPrimary},
    };
    const int activeTab = std::clamp(
        pauseMenu.activePageIndex,
        0,
        static_cast<int>(ui::pause_menu_layout::kPageTabLabels.size()) - 1);

    for (std::size_t i = 0; i < ui::pause_menu_layout::kPageTabLabels.size(); ++i) {
        drawButtonPx(
            pauseButtonStyle,
            l.tabRects[i],
            tabPadX,
            tabPadY,
            tabsScalePx,
            ui::pause_menu_layout::kPageTabLabels[i],
            pauseMenu.hoveredPageTabIndex == static_cast<int>(i),
            static_cast<int>(i) == activeTab);
    }

    for (const ActionButtonDef button :
         {ActionButtonDef{OverlayRenderer::PauseMenuAction::Exit, "EXIT TO HOME"},
          ActionButtonDef{OverlayRenderer::PauseMenuAction::Close, "X"},
          ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetIcon, "R"}})
    {
        const auto& state = pauseMenu.actions[actionIndex(button.action)];
        if (!state.visible) {
            continue;
        }
        drawButtonPx(
            pauseButtonStyle,
            l.actionRect(button.action),
            12.0f * menuScale,
            tabPadY,
            tabsScalePx,
            button.label,
            state.hovered,
            state.selected);
    }

    pushQuadPx(buffers.panelFill, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1);
    pushFramePx(buffers.panelFrame, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1, 1.5f);
    pushQuadPx(buffers.accentFill, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY0 + 3.0f);
    appendTextPx(
        buffers.textAccent,
        l.sectionX0 + 14.0f * menuScale,
        l.settingsY0 + 10.0f * menuScale,
        l.sectionHeaderScalePx,
        "SETTINGS");

    if (l.visibleLines > 0 && l.totalLines > l.visibleLines) {
        const float trackW = 8.0f * menuScale;
        const float trackX1 = l.sectionX1 - 8.0f * menuScale;
        const float trackX0 = trackX1 - trackW;
        const float trackY0 = l.linesStartY;
        const float trackY1 = l.contentY1;
        pushQuadPx(buffers.panelFill, trackX0, trackY0, trackX1, trackY1);
        pushFramePx(buffers.panelFrame, trackX0, trackY0, trackX1, trackY1, 1.0f);
        const float visibleRatio =
            std::clamp(static_cast<float>(l.visibleLines) / static_cast<float>(l.totalLines), 0.0f, 1.0f);
        const float thumbH = std::max(18.0f * menuScale, (trackY1 - trackY0) * visibleRatio);
        const float maxFirst = static_cast<float>(l.totalLines - l.visibleLines);
        const float scrollT = maxFirst > 0.0f ? static_cast<float>(l.firstLine) / maxFirst : 0.0f;
        const float thumbTravel = std::max(0.0f, (trackY1 - trackY0) - thumbH);
        const float thumbY0 = trackY0 + thumbTravel * scrollT;
        pushQuadPx(
            buffers.accentFill,
            trackX0 + 1.0f,
            thumbY0 + 1.0f,
            trackX1 - 1.0f,
            thumbY0 + thumbH - 1.0f);
    }

    for (const auto [action, label] :
         {ActionButtonDef{OverlayRenderer::PauseMenuAction::Apply, "APPLY CHANGES"},
          ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetWorld, "RESET WORLD"},
          ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetControls, "RESET CONTROLS"}})
    {
        const auto& state = pauseMenu.actions[actionIndex(action)];
        if (!state.visible) {
            continue;
        }
        drawButtonPx(
            pauseButtonStyle,
            l.actionRect(action),
            16.0f * menuScale,
            8.0f * menuScale,
            l.actionScalePx,
            label,
            state.hovered,
            state.selected);
    }

    for (std::size_t i = 0; i < l.visibleLines; ++i) {
        const std::size_t lineIdx = l.firstLine + i;
        const auto& line = pauseMenu.lines[lineIdx];
        const bool selected = static_cast<int>(lineIdx) == pauseMenu.selectedSettingLineIndex;
        const bool hovered = static_cast<int>(lineIdx) == pauseMenu.hoveredSettingLineIndex;
        const bool disabled = line.disabled;
        const auto widgets = l.lineWidgets(i, line);
        const float lineY = l.linesStartY + static_cast<float>(i) * l.rowStep;

        if (selected && !line.header) {
            pushFramePx(
                buffers.textPrimary,
                widgets.line.x0,
                widgets.line.y0,
                widgets.line.x1,
                widgets.line.y1,
                1.5f);
        } else if (hovered && !line.header) {
            pushQuadPx(buffers.panelFrame, widgets.line.x0, widgets.line.y0, widgets.line.x1, widgets.line.y1);
        }

        const float lineScalePx = line.header
            ? fitScaleForWidth(line.text, l.rowScalePx * 1.03f, (l.sectionX1 - l.sectionX0))
            : l.rowScalePx;
        appendTextPx(
            line.header ? buffers.textAccent
                        : (disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted)),
            l.sectionX0 + 14.0f * menuScale,
            lineY,
            lineScalePx,
            line.text);
        if (line.header) {
            continue;
        }

        const float valueScalePx = l.rowScalePx * 0.88f;
        const float buttonY0 = lineY - 2.0f * menuScale;
        const float buttonY1 =
            lineY + (static_cast<float>(kFontH) + 3.5f) * l.rowScalePx + 2.0f * menuScale;
        const auto drawInlineButton =
            [&](const float x0, const float x1, const std::string& text, const bool active, const bool warning) {
                if (active && !disabled && !selected) {
                    pushQuadPx(buffers.panelFill, x0, buttonY0, x1, buttonY1);
                }
                if (active) {
                    pushFramePx(
                        disabled ? buffers.panelFrame : buffers.textPrimary,
                        x0,
                        buttonY0,
                        x1,
                        buttonY1,
                        1.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted
                             : (active ? buffers.textPrimary
                                       : (warning ? buffers.textWarning : buffers.textMuted)),
                    x0 + (x1 - x0 - measureMaxLinePx(text, valueScalePx)) * 0.5f,
                    lineY + 0.3f * menuScale,
                    valueScalePx,
                    text);
            };

        switch (line.controlType) {
            case OverlayRenderer::PauseMenuControlType::Toggle: {
                const std::string toggleText = line.boolValue ? "ON" : "OFF";
                const float textX0 = widgets.checkbox.x1 + 5.0f * menuScale;
                const float textW = measureMaxLinePx(toggleText, valueScalePx);
                const float textX =
                    textX0 + std::max(0.0f, (widgets.value.x1 - 12.0f * menuScale - textX0 - textW) * 0.5f);
                pushFramePx(
                    buffers.textMuted,
                    widgets.checkbox.x0,
                    widgets.checkbox.y0,
                    widgets.checkbox.x1,
                    widgets.checkbox.y1,
                    1.1f);
                if (line.boolValue) {
                    pushQuadPx(
                        disabled ? buffers.panelFrame : buffers.accentFill,
                        widgets.checkbox.x0 + 2.0f,
                        widgets.checkbox.y0 + 2.0f,
                        widgets.checkbox.x1 - 2.0f,
                        widgets.checkbox.y1 - 2.0f);
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
            case OverlayRenderer::PauseMenuControlType::Numeric:
                if (disabled) {
                    appendTextPx(
                        buffers.textMuted,
                        widgets.value.x0 +
                            ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.valueText, valueScalePx)) *
                                0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.valueText);
                } else {
                    const bool numeric = line.controlType == OverlayRenderer::PauseMenuControlType::Numeric;
                    drawInlineButton(widgets.leftArrow.x0, widgets.leftArrow.x1, numeric ? "-" : "<", false, false);
                    drawInlineButton(widgets.rightArrow.x0, widgets.rightArrow.x1, numeric ? "+" : ">", false, false);
                    drawInlineButton(widgets.value.x0, widgets.value.x1, line.valueText, false, false);
                }
                break;
            case OverlayRenderer::PauseMenuControlType::Rebind:
            case OverlayRenderer::PauseMenuControlType::Action:
            case OverlayRenderer::PauseMenuControlType::None:
                if (!line.valueText.empty()) {
                    appendTextPx(
                        buffers.textMuted,
                        widgets.value.x0 +
                            ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.valueText, valueScalePx)) *
                                0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.valueText);
                }
                break;
        }
    }

    if (!pauseMenu.footerHint.empty()) {
        const float footerX0 = l.sectionX0;
        const float footerY0 = l.settingsY1 + 10.0f * menuScale;
        const float footerY1 = cardY1 - 16.0f * menuScale;
        pushQuadPx(buffers.panelFill, footerX0, footerY0, l.sectionX1, footerY1);
        pushFramePx(buffers.panelFrame, footerX0, footerY0, l.sectionX1, footerY1, 1.2f);
        appendTextPx(
            buffers.textMuted,
            footerX0 + 10.0f * menuScale,
            footerY0 + 6.0f * menuScale,
            fitScaleForWidth(
                pauseMenu.footerHint,
                baseScalePx * 0.92f,
                (l.sectionX1 - footerX0) - 18.0f * menuScale),
            pauseMenu.footerHint);
    }

    if (!pauseMenu.showResetConfirm) {
        return;
    }

    const auto popupButtons = ui::pause_menu_layout::buildPopupButtonsLayout(l, pauseMenu);
    const bool singleAcknowledge = popupButtons.singleAcknowledge;
    const std::string titleText =
        pauseMenu.resetConfirmTitleText.empty()
            ? (singleAcknowledge ? "WARNING" : "ARE YOU SURE?")
            : pauseMenu.resetConfirmTitleText;
    const std::string bodyText =
        pauseMenu.resetConfirmBodyText.empty() ? "RESET ALL SETTINGS" : pauseMenu.resetConfirmBodyText;

    pushQuadPx(buffers.popupBg, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1);
    pushFramePx(buffers.popupFrame, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1, 1.5f);
    pushQuadPx(buffers.popupAccent, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y0 + 3.0f);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 14.0f * menuScale,
        baseScalePx * 0.98f,
        titleText);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 36.0f * menuScale,
        popupButtons.buttonScalePx,
        bodyText);

    if (pauseMenu.hoverResetConfirmYes && !pauseMenu.selectedResetConfirmYes) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.yesButton.x0 + 1.0f,
            popupButtons.yesButton.y0 + 1.0f,
            popupButtons.yesButton.x1 - 1.0f,
            popupButtons.yesButton.y1 - 1.0f);
    }
    pushFramePx(
        pauseMenu.selectedResetConfirmYes ? buffers.popupText : buffers.popupFrame,
        popupButtons.yesButton.x0,
        popupButtons.yesButton.y0,
        popupButtons.yesButton.x1,
        popupButtons.yesButton.y1,
        pauseMenu.selectedResetConfirmYes ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.yesButton.x0 + popupButtons.buttonPadX,
        popupButtons.yesButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.yesLabel);
    if (singleAcknowledge) {
        return;
    }

    if (pauseMenu.hoverResetConfirmNo && !pauseMenu.selectedResetConfirmNo) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.noButton.x0 + 1.0f,
            popupButtons.noButton.y0 + 1.0f,
            popupButtons.noButton.x1 - 1.0f,
            popupButtons.noButton.y1 - 1.0f);
    }
    pushFramePx(
        pauseMenu.selectedResetConfirmNo ? buffers.popupText : buffers.popupFrame,
        popupButtons.noButton.x0,
        popupButtons.noButton.y0,
        popupButtons.noButton.x1,
        popupButtons.noButton.y1,
        pauseMenu.selectedResetConfirmNo ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.noButton.x0 + popupButtons.buttonPadX,
        popupButtons.noButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.noLabel);
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
    const float hudRowStep = 20.0f * geometry.uiScale;
    constexpr float hudX0 = 16.0f;
    constexpr float hudY0 = 16.0f;
    constexpr float hudTextX = hudX0 + 10.0f;
    constexpr float hudTextY0 = hudY0 + 10.0f;
    char speedLine[64];
    char fpsLine[64];
    formatHudSpeed(speedLine, sizeof(speedLine), simSpeed);
    std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);

    float maxLineW = std::max(
        {measureMaxLinePx(simFrozen ? "WORLD: PAUSED" : "WORLD: RUNNING", baseScalePx),
         measureMaxLinePx("ESC: MENU", baseScalePx),
         measureMaxLinePx(speedLine, baseScalePx),
         measureMaxLinePx(fpsLine, baseScalePx)});
    for (const auto& line : hudDebugLines) {
        maxLineW = std::max(maxLineW, measureMaxLinePx(line, baseScalePx));
    }

    const float hudX1 = std::min(geometry.width - 16.0f, hudTextX + maxLineW + 20.0f);
    const float hudY1 = hudY0 + 16.0f + (4.0f + static_cast<float>(hudDebugLines.size())) * hudRowStep;
    pushQuadPx(buffers.panelFill, hudX0, hudY0, hudX1, hudY1);
    pushFramePx(buffers.panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);
    pushQuadPx(buffers.accentFill, hudX0, hudY0, hudX1, hudY0 + 3.0f);
    appendTextPx(
        buffers.statusText,
        hudTextX,
        hudTextY0,
        baseScalePx,
        simFrozen ? "WORLD: PAUSED" : "WORLD: RUNNING");
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

void drawCrosshair(const Geometry& geometry, const bool simFrozen, const Buffers& buffers) {
    const float cx = geometry.width * 0.5f;
    const float cy = geometry.height * 0.5f;
    if (simFrozen) {
        pushQuadPx(buffers.crosshair, cx - 6.0f, cy - 12.0f, cx - 2.0f, cy + 12.0f);
        pushQuadPx(buffers.crosshair, cx + 2.0f, cy - 12.0f, cx + 6.0f, cy + 12.0f);
        return;
    }
    pushQuadPx(buffers.crosshair, cx - 13.0f, cy - 1.0f, cx - 5.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx + 5.0f, cy - 1.0f, cx + 13.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 13.0f, cx + 1.0f, cy - 5.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy + 5.0f, cx + 1.0f, cy + 13.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
}

void drawPathLines(
    const Geometry& geometry,
    const std::vector<OverlayRenderer::ScreenLine>& pathLines,
    const Buffers& buffers)
{
    (void)geometry;
    for (const auto& line : pathLines) {
        pushLinePx(buffers.pathLines, line.x0, line.y0, line.x1, line.y1);
    }
}

void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers)
{
    if (!targetHud.visible || targetHud.lines.empty()) {
        return;
    }

    const float baseScalePx = kBaseScalePx * geometry.uiScale * 0.88f;
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
    const float popupH =
        static_cast<float>((kFontH + 2) * static_cast<int>(targetHud.lines.size())) * baseScalePx +
        12.0f;
    const float px = targetHud.xPx - popupW * 0.5f;
    const float py = targetHud.yPx - popupH - 12.0f;
    pushQuadPx(buffers.popupBg, px, py, px + popupW, py + popupH);
    pushFramePx(buffers.popupFrame, px, py, px + popupW, py + popupH, 1.5f);
    pushQuadPx(buffers.accentFill, px, py, px + popupW, py + 3.0f);
    appendTextPx(buffers.popupText, px + 8.0f, py + 6.0f, baseScalePx, popup);
}

} // namespace overlay_renderer

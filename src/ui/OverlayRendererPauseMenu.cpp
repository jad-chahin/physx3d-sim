#include "ui/OverlayRendererDrawShared.h"

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
}

void drawPauseMenu(
    const OverlayRenderer::PauseMenuHud& pauseMenu,
    const Geometry& geometry,
    const Buffers& buffers)
{
    const float w = geometry.width;
    const float h = geometry.height;
    const ui::pause_menu_shared::PauseMenuLayout sharedLayout =
        ui::pause_menu_shared::buildPauseMenuLayout(w, h, geometry.uiScale, pauseMenu);
    const float menuScale = sharedLayout.menuScale;
    const float baseScalePx = sharedLayout.baseScalePx;
    const float preferredTitleScalePx = kTitleScalePx * menuScale;

    pushQuadPx(buffers.screenDim, 0.0f, 0.0f, w, h);

    const float cardX0 = sharedLayout.cardX0;
    const float cardY0 = sharedLayout.cardY0;
    const float cardX1 = sharedLayout.cardX1;
    const float cardY1 = sharedLayout.cardY1;
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

    const float statusSlotY = cardY0 + 96.0f * menuScale;
    if (!pauseMenu.statusLine.empty()) {
        const float statusScalePx = fitScaleForWidth(pauseMenu.statusLine, baseScalePx * 0.95f, infoWidth);
        appendTextPx(
            pauseMenu.awaitingBind ? buffers.textWarning : buffers.textAccent,
            centerX - measureMaxLinePx(pauseMenu.statusLine, statusScalePx) * 0.5f,
            statusSlotY,
            statusScalePx,
            pauseMenu.statusLine);
    }

    const float tabsScalePx = sharedLayout.tabsScalePx;
    const float tabPadX = 12.0f * menuScale;
    const float tabPadY = 7.0f * menuScale;
    const ButtonStyle pauseButtonStyle{
        {buffers.panelFill, buffers.panelFrame, buffers.textMuted},
        {buffers.panelFill, buffers.textPrimary, buffers.textPrimary},
        {buffers.panelFrame, buffers.textPrimary, buffers.textPrimary},
    };
    const int activeTab = std::clamp(
        pauseMenu.activePageIndex,
        0,
        static_cast<int>(ui::pause_menu_shared::kPageTabLabels.size()) - 1);
    for (std::size_t i = 0; i < ui::pause_menu_shared::kPageTabLabels.size(); ++i) {
        drawButtonPx(
            pauseButtonStyle,
            sharedLayout.tabRects[i],
            tabPadX,
            tabPadY,
            tabsScalePx,
            ui::pause_menu_shared::kPageTabLabels[i],
            pauseMenu.hoveredPageTabIndex == static_cast<int>(i),
            static_cast<int>(i) == activeTab);
    }

    for (const ActionButtonDef button : {
             ActionButtonDef{OverlayRenderer::PauseMenuAction::Exit, "EXIT TO HOME"},
             ActionButtonDef{OverlayRenderer::PauseMenuAction::Close, "X"},
             ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetIcon, "R"} })
    {
        const auto& state = pauseMenu.actions[actionIndex(button.action)];
        if (!state.visible) {
            continue;
        }
        drawButtonPx(
            pauseButtonStyle,
            sharedLayout.actionRect(button.action),
            12.0f * menuScale,
            tabPadY,
            tabsScalePx,
            button.label,
            state.hovered,
            state.selected);
    }

    const float sectionX0 = sharedLayout.sectionX0;
    const float sectionX1 = sharedLayout.sectionX1;
    const float settingsY0 = sharedLayout.settingsY0;
    const float settingsY1 = sharedLayout.settingsY1;
    const float popupX0 = sharedLayout.popupRect.x0;
    const float popupY0 = sharedLayout.popupRect.y0;
    const float popupX1 = sharedLayout.popupRect.x1;
    const float popupY1 = sharedLayout.popupRect.y1;

    pushQuadPx(buffers.panelFill, sectionX0, settingsY0, sectionX1, settingsY1);
    pushFramePx(buffers.panelFrame, sectionX0, settingsY0, sectionX1, settingsY1, 1.5f);
    pushQuadPx(buffers.accentFill, sectionX0, settingsY0, sectionX1, settingsY0 + 3.0f);

    const float rowScalePx = sharedLayout.rowScalePx;
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
            case OverlayRenderer::PauseMenuControlType::None:
                break;
        }
    }
    const float rowAreaWidth = (sectionX1 - sectionX0) - 28.0f * menuScale - maxControlReserve;

    appendTextPx(
        buffers.textAccent,
        sectionX0 + 14.0f * menuScale,
        settingsY0 + 10.0f * menuScale,
        sharedLayout.sectionHeaderScalePx,
        "SETTINGS");

    const float linesStartY = sharedLayout.linesStartY;
    const float rowStep = sharedLayout.rowStep;
    const float actionScalePx = sharedLayout.actionScalePx;
    const float actionPadY = 8.0f * menuScale;
    const float contentY1 = sharedLayout.contentY1;
    const std::size_t totalLines = sharedLayout.totalLines;
    const std::size_t visibleLines = sharedLayout.visibleLines;
    const std::size_t firstLine = sharedLayout.firstLine;

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
        const float scrollT = maxFirst > 0.0f ? static_cast<float>(firstLine) / maxFirst : 0.0f;
        const float thumbTravel = std::max(0.0f, (trackY1 - trackY0) - thumbH);
        const float thumbY0 = trackY0 + thumbTravel * scrollT;
        const float thumbY1 = thumbY0 + thumbH;
        pushQuadPx(buffers.accentFill, trackX0 + 1.0f, thumbY0 + 1.0f, trackX1 - 1.0f, thumbY1 - 1.0f);
    }

    for (const auto [action, label] : {
             ActionButtonDef{OverlayRenderer::PauseMenuAction::Apply, "APPLY CHANGES"},
             ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetWorld, "RESET WORLD"},
             ActionButtonDef{OverlayRenderer::PauseMenuAction::ResetControls, "RESET CONTROLS"} })
    {
        const auto& state = pauseMenu.actions[actionIndex(action)];
        if (!state.visible) {
            continue;
        }
        drawButtonPx(
            pauseButtonStyle,
            sharedLayout.actionRect(action),
            16.0f * menuScale,
            actionPadY,
            actionScalePx,
            label,
            state.hovered,
            state.selected);
    }

    for (std::size_t i = 0; i < visibleLines; ++i) {
        const std::size_t lineIdx = firstLine + i;
        const auto& line = pauseMenu.lines[lineIdx];
        const bool selected = static_cast<int>(lineIdx) == pauseMenu.selectedSettingLineIndex;
        const bool hovered = static_cast<int>(lineIdx) == pauseMenu.hoveredSettingLineIndex;
        const bool disabled = line.disabled;
        const auto widgets = sharedLayout.lineWidgets(i, line);
        const float lineY = linesStartY + static_cast<float>(i) * rowStep;

        if (selected && !line.header) {
            pushFramePx(buffers.textPrimary, widgets.line.x0, widgets.line.y0, widgets.line.x1, widgets.line.y1, 1.5f);
        } else if (hovered && !line.header) {
            pushFramePx(buffers.panelFrame, widgets.line.x0, widgets.line.y0, widgets.line.x1, widgets.line.y1, 1.0f);
        }

        const float lineScalePx = line.header
            ? fitScaleForWidth(line.text, rowScalePx * 1.03f, rowAreaWidth + maxControlReserve)
            : rowScalePx;
        appendTextPx(
            line.header ? buffers.textAccent : (disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted)),
            sectionX0 + 14.0f * menuScale,
            lineY,
            lineScalePx,
            line.text);
        if (line.header) {
            continue;
        }

        const float valueScalePx = rowScalePx * 0.88f;
        const float buttonY0 = lineY - 2.0f * menuScale;
        const float buttonY1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale;
        const auto drawInlineButton = [&](const float x0, const float x1, const std::string& text, const bool active, const bool warning) {
            if (active && !disabled && !selected) {
                pushQuadPx(buffers.panelFill, x0, buttonY0, x1, buttonY1);
            }
            if (active) {
                pushFramePx(disabled ? buffers.panelFrame : buffers.textPrimary, x0, buttonY0, x1, buttonY1, 1.0f);
            }
            const float tx = x0 + (x1 - x0 - measureMaxLinePx(text, valueScalePx)) * 0.5f;
            appendTextPx(
                disabled ? buffers.textMuted : (active ? buffers.textPrimary : (warning ? buffers.textWarning : buffers.textMuted)),
                tx,
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
                pushFramePx(buffers.textMuted, widgets.checkbox.x0, widgets.checkbox.y0, widgets.checkbox.x1, widgets.checkbox.y1, 1.1f);
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
            case OverlayRenderer::PauseMenuControlType::Numeric: {
                const bool numeric = line.controlType == OverlayRenderer::PauseMenuControlType::Numeric;
                const char* leftSymbol = numeric ? "-" : "<";
                const char* rightSymbol = numeric ? "+" : ">";
                if (disabled) {
                    appendTextPx(
                        buffers.textMuted,
                        widgets.value.x0 + ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.valueText, valueScalePx)) * 0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.valueText);
                } else {
                    drawInlineButton(widgets.leftArrow.x0, widgets.leftArrow.x1, leftSymbol, false, false);
                    drawInlineButton(widgets.rightArrow.x0, widgets.rightArrow.x1, rightSymbol, false, false);
                    drawInlineButton(widgets.value.x0, widgets.value.x1, line.valueText, false, false);
                }
                if (numeric && widgets.hasSlider && !disabled) {
                    pushQuadPx(buffers.panelFrame, widgets.slider.x0, widgets.slider.y0, widgets.slider.x1, widgets.slider.y1);
                    const float fillX = widgets.slider.x0 +
                        std::clamp(line.sliderT, 0.0f, 1.0f) * (widgets.slider.x1 - widgets.slider.x0);
                    pushQuadPx(buffers.accentFill, widgets.slider.x0, widgets.slider.y0, fillX, widgets.slider.y1);
                }
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Rebind:
                appendTextPx(
                    buffers.textMuted,
                    widgets.value.x0 + ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.valueText, valueScalePx)) * 0.5f,
                    lineY + 0.3f * menuScale,
                    valueScalePx,
                    line.valueText);
                break;
            case OverlayRenderer::PauseMenuControlType::Action:
                if (!line.valueText.empty()) {
                    appendTextPx(
                        selected ? buffers.textPrimary : buffers.textMuted,
                        widgets.value.x0 + ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.valueText, valueScalePx)) * 0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.valueText);
                }
                break;
            case OverlayRenderer::PauseMenuControlType::None:
                if (!line.valueText.empty()) {
                    appendTextPx(buffers.textMuted, widgets.value.x0, lineY + 0.3f * menuScale, valueScalePx, line.valueText);
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

    if (!pauseMenu.showResetConfirm) {
        return;
    }

    const ui::pause_menu_shared::PopupButtonsLayout popupButtons =
        ui::pause_menu_shared::buildPopupButtonsLayout(sharedLayout, pauseMenu);
    const bool singleAcknowledge = popupButtons.singleAcknowledge;
    const std::string titleText = pauseMenu.resetConfirmTitleText.empty()
        ? (singleAcknowledge ? "WARNING" : "ARE YOU SURE?")
        : pauseMenu.resetConfirmTitleText;
    const std::string bodyText = pauseMenu.resetConfirmBodyText.empty()
        ? "RESET ALL SETTINGS"
        : pauseMenu.resetConfirmBodyText;

    pushQuadPx(buffers.popupBg, popupX0, popupY0, popupX1, popupY1);
    pushFramePx(buffers.popupFrame, popupX0, popupY0, popupX1, popupY1, 1.5f);
    pushQuadPx(buffers.popupAccent, popupX0, popupY0, popupX1, popupY0 + 3.0f);
    appendTextPx(buffers.popupText, popupX0 + 16.0f * menuScale, popupY0 + 14.0f * menuScale, baseScalePx * 0.98f, titleText);
    appendTextPx(
        buffers.popupText,
        popupX0 + 16.0f * menuScale,
        popupY0 + 36.0f * menuScale,
        popupButtons.buttonScalePx,
        bodyText);

    if (pauseMenu.selectedResetConfirmYes) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.yesButton.x0,
            popupButtons.yesButton.y0,
            popupButtons.yesButton.x1,
            popupButtons.yesButton.y1);
    }
    pushFramePx(
        pauseMenu.hoverResetConfirmYes || pauseMenu.selectedResetConfirmYes ? buffers.popupText : buffers.popupFrame,
        popupButtons.yesButton.x0,
        popupButtons.yesButton.y0,
        popupButtons.yesButton.x1,
        popupButtons.yesButton.y1,
        1.5f);
    appendTextPx(
        buffers.popupText,
        popupButtons.yesButton.x0 + popupButtons.buttonPadX,
        popupButtons.yesButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.yesLabel);

    if (singleAcknowledge) {
        return;
    }
    if (pauseMenu.selectedResetConfirmNo) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.noButton.x0,
            popupButtons.noButton.y0,
            popupButtons.noButton.x1,
            popupButtons.noButton.y1);
    }
    pushFramePx(
        pauseMenu.hoverResetConfirmNo || pauseMenu.selectedResetConfirmNo ? buffers.popupText : buffers.popupFrame,
        popupButtons.noButton.x0,
        popupButtons.noButton.y0,
        popupButtons.noButton.x1,
        popupButtons.noButton.y1,
        1.5f);
    appendTextPx(
        buffers.popupText,
        popupButtons.noButton.x0 + popupButtons.buttonPadX,
        popupButtons.noButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.noLabel);
}

} // namespace overlay_renderer

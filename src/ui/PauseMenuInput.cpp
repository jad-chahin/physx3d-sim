#include "ui/PauseMenuController.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cmath>

#include "ui/OverlayRendererDrawShared.h"

namespace ui {
    using namespace pause_menu_shared;
    using PauseAction = OverlayRenderer::PauseMenuAction;

    namespace {
        constexpr std::size_t actionIndex(const PauseAction action) {
            return static_cast<std::size_t>(action);
        }
    }

    void PauseMenuController::updateEscapeState(
        GLFWwindow* window,
        bool& mouseCaptured,
        bool& firstMouse,
        double& lastFrameTime,
        double& accumulator,
        double& alpha,
        std::vector<sim::Body>& bodies)
    {
        if (window == nullptr) {
            return;
        }

        const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool backDown = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        if ((escDown && !escWasDown_) || (backDown && !backWasDown_) || resumeRequested_) {
            if (open_ && awaitingRebind_) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_.clear();
                resumeRequested_ = false;
                escWasDown_ = escDown;
                backWasDown_ = backDown;
                return;
            }
            if (open_ && hasOpenPopup())
            {
                closePopup();
                resumeRequested_ = false;
                escWasDown_ = escDown;
                backWasDown_ = backDown;
                return;
            }

            const bool triggeredByResume = resumeRequested_;
            open_ = !open_;
            if (triggeredByResume) {
                open_ = false;
            }
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            ignoreNextRebindMousePress_ = false;
            resumeRequested_ = false;
            clearHoverState();
            popupType_ = PopupType::None;
            discardPendingEdit();
            statusMessage_.clear();
            upHeld_ = false;
            downHeld_ = false;
            leftHeld_ = false;
            rightHeld_ = false;

            if (open_) {
                setSelectedSettingRow(-1);
                focusTarget_ = FocusTarget::SettingsRow;
                scrollLineOffset_ = 0;

                mouseCaptured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                mouseCaptured = true;
                firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                lastFrameTime = glfwGetTime();
                accumulator = 0.0;
                alpha = 0.0;
                for (auto& body : bodies) {
                    body.prevPosition = body.position;
                    body.prevOrientation = body.orientation;
                }
            }
        }

        escWasDown_ = escDown;
        backWasDown_ = backDown;
    }

    void PauseMenuController::handlePressedKey(
        GLFWwindow* window,
        const int pressedKey,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        const bool controlsDirty = controls != input::ControlBindings{};
        if (!open_ || window == nullptr) {
            return;
        }

        if (awaitingRebind_) {
            if (pressedKey == GLFW_KEY_ESCAPE || pressedKey == GLFW_KEY_BACKSPACE) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_ = "REBIND CANCELED";
                return;
            }

            if (pressedKey != GLFW_KEY_UNKNOWN) {
                applyRebindCode(pressedKey, controls, controlsConfigPath);
            }
            return;
        }

        switch (pressedKey) {
            case GLFW_KEY_TAB:
                switchPage((glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) ? -1 : 1);
                return;
            case GLFW_KEY_UP:
                moveVerticalFocus(-1, controlsDirty);
                return;
            case GLFW_KEY_DOWN:
                moveVerticalFocus(1, controlsDirty);
                return;
            case GLFW_KEY_LEFT:
                moveHorizontalFocus(-1);
                return;
            case GLFW_KEY_RIGHT:
                moveHorizontalFocus(1);
                return;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
            case GLFW_KEY_SPACE:
                activateFocusedControl(window, controls, controlsConfigPath);
                return;
            default:
                return;
        }
    }

    void PauseMenuController::handlePointerInput(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath,
        const float scrollDeltaY)
    {
        if (window == nullptr) {
            return;
        }

        const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clickPressed = leftMouseDown && !leftMouseWasDown_;
        leftMouseWasDown_ = leftMouseDown;
        int pressedMouseBindingCode = GLFW_KEY_UNKNOWN;
        for (int button = 0; button < input::kMouseBindingMaxButtons; ++button) {
            const bool down = glfwGetMouseButton(window, button) == GLFW_PRESS;
            if (down && !mouseButtonWasDown_[static_cast<std::size_t>(button)] &&
                pressedMouseBindingCode == GLFW_KEY_UNKNOWN)
            {
                pressedMouseBindingCode = input::bindingCodeFromMouseButton(button);
            }
            mouseButtonWasDown_[static_cast<std::size_t>(button)] = down;
        }

        if (!open_) {
            clearHoverState();
            return;
        }

        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        int ww = 0;
        int wh = 0;
        glfwGetWindowSize(window, &ww, &wh);
        if (fbw <= 0 || fbh <= 0 || ww <= 0 || wh <= 0) {
            return;
        }

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        const auto xPx = static_cast<float>(cursorX * (static_cast<double>(fbw) / static_cast<double>(ww)));
        const auto yPx = static_cast<float>(cursorY * (static_cast<double>(fbh) / static_cast<double>(wh)));

        const auto w = static_cast<float>(fbw);
        const auto h = static_cast<float>(fbh);
        constexpr std::array kPageOrder{
            SettingsPage::Display, SettingsPage::Simulation, SettingsPage::Camera, SettingsPage::Interface, SettingsPage::Controls,
        };
        constexpr std::array kHoverActions{
            PauseAction::ResetIcon,
            PauseAction::Close,
            PauseAction::Exit,
            PauseAction::Apply,
            PauseAction::ResetWorld,
            PauseAction::ResetControls,
        };

        const OverlayRenderer::PauseMenuHud hud = buildHud(controls, false);
        if (hud.lines.empty()) {
            clearHoverState();
            return;
        }
        const PauseMenuLayout layout = buildPauseMenuLayout(w, h, uiScale(), hud);
        const float cardX0 = layout.cardX0;
        const float cardX1 = layout.cardX1;
        const float settingsY0 = layout.settingsY0;
        const float settingsY1 = layout.settingsY1;
        const std::size_t totalLines = layout.totalLines;
        const std::size_t visibleLines = layout.visibleLines;
        const std::size_t firstLine = layout.firstLine;

        const int count = settingCount();
        const auto firstSelectableIt = std::ranges::find_if(hud.lines, [](const auto& line) {
            return !line.header;
        });
        const int firstSelectableLine = firstSelectableIt == hud.lines.end()
            ? -1
            : static_cast<int>(std::distance(hud.lines.begin(), firstSelectableIt));
        if (firstSelectableLine < 0) {
            clearHoverState();
            return;
        }

        struct HoverHit {
            int line = -1;
            int visible = -1;
            int row = -1;
        };
        const auto findHoveredLine = [&]() {
            HoverHit hit{};
            for (std::size_t i = 0; i < visibleLines; ++i) {
                const std::size_t lineIdx = firstLine + i;
                if (!layout.lineRect(i).contains(xPx, yPx)) {
                    continue;
                }
                if (hud.lines[lineIdx].header) {
                    break;
                }
                const int rowIndex = static_cast<int>(lineIdx) - firstSelectableLine;
                if (rowIndex < 0 || rowIndex >= count) {
                    break;
                }
                hit.line = static_cast<int>(lineIdx);
                hit.visible = static_cast<int>(i);
                hit.row = rowIndex;
                break;
            }
            return hit;
        };
        const auto [line, visible, row] = findHoveredLine();

        clearHoverState();
        hoveredSettingRow_ = row;

        for (std::size_t i = 0; i < kPageOrder.size(); ++i) {
            if (layout.tabRects[i].contains(xPx, yPx)) {
                hoveredPageTabIndex_ = static_cast<int>(i);
                break;
            }
        }
        for (const auto action : kHoverActions) {
            const auto idx = actionIndex(action);
            hoveredActions_[idx] = hud.actions[idx].visible && layout.actionRect(action).contains(xPx, yPx);
        }

        if (awaitingRebind_) {
            if (pressedMouseBindingCode == GLFW_KEY_UNKNOWN) {
                return;
            }
            if (ignoreNextRebindMousePress_) {
                ignoreNextRebindMousePress_ = false;
                return;
            }
            if (row != selectedSettingRow_) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_ = "REBIND CANCELED";
                return;
            }
            applyRebindCode(pressedMouseBindingCode, controls, controlsConfigPath);
            return;
        }

        if (std::fabs(scrollDeltaY) > 0.01f) {
            const int wheelDirection = scrollDeltaY > 0.0f ? -1 : 1;
            const int wheelSteps = std::min(4, std::max(1, static_cast<int>(std::fabs(scrollDeltaY))));
            if (visibleLines > 0 && totalLines > visibleLines) {
                const int maxFirstLine = static_cast<int>(totalLines - visibleLines);
                if (selectedSettingRow_ >= 0) {
                    discardPendingEdit();
                    setSelectedSettingRow(-1);
                    statusMessage_.clear();
                }
                scrollLineOffset_ = std::clamp(scrollLineOffset_ + wheelDirection * wheelSteps, 0, maxFirstLine);
            }
            return;
        }

        if (hasOpenPopup()) {
            const PopupButtonsLayout popupButtons = buildPopupButtonsLayout(layout, hud);
            hoveredResetConfirmYes_ = popupButtons.yesButton.contains(xPx, yPx);
            hoveredResetConfirmNo_ = !popupButtons.singleAcknowledge && popupButtons.noButton.contains(xPx, yPx);

            if (!clickPressed) {
                return;
            }

            if (hoveredResetConfirmYes_) {
                confirmPopup(window);
            } else if (hoveredResetConfirmNo_ ||
                       !layout.popupRect.contains(xPx, yPx))
            {
                closePopup();
            }
            return;
        }

        if (!clickPressed) {
            return;
        }

        const double clickAt = glfwGetTime();
        constexpr double kDoubleClickWindowSeconds = 0.35;

        const auto actionHovered = [&](const PauseAction action) {
            return hoveredActions_[actionIndex(action)];
        };

        if (actionHovered(PauseAction::Close)) {
            resumeRequested_ = true;
            return;
        }
        if (actionHovered(PauseAction::Exit)) {
            openPopup(PopupType::ExitToHome, FocusTarget::ExitButton, false);
            return;
        }

        for (std::size_t i = 0; i < kPageOrder.size(); ++i) {
            if (layout.tabRects[i].contains(xPx, yPx)) {
                switchToPage(kPageOrder[i]);
                return;
            }
        }

        if (popupType_ != PopupType::ResetSettings && hoveredActions_[actionIndex(PauseAction::ResetIcon)]) {
            openPopup(PopupType::ResetSettings, FocusTarget::ResetIcon, false);
            return;
        }

        if (row < 0 || line < 0) {
            if (actionHovered(PauseAction::Apply)) {
                commitSelectedSetting(window);
                return;
            }
            if (actionHovered(PauseAction::ResetWorld)) {
                openPopup(PopupType::ResetWorld, FocusTarget::ResetWorldAction, false);
                return;
            }
            if (actionHovered(PauseAction::ResetControls)) {
                resetControlsToDefaults(controls, controlsConfigPath);
                return;
            }
            if (selectedSettingRow_ >= 0 &&
                xPx >= cardX0 && xPx <= cardX1 &&
                yPx >= settingsY0 && yPx <= settingsY1)
            {
                discardPendingEdit();
                setSelectedSettingRow(-1);
                statusMessage_.clear();
            }
            return;
        }

        const bool isDoubleClick =
            lastClickedPage_ == settingsPage_ &&
            lastClickedRow_ == row &&
            lastClickAt_ >= 0.0 &&
            (clickAt - lastClickAt_) <= kDoubleClickWindowSeconds;
        lastClickedPage_ = settingsPage_;
        lastClickedRow_ = row;
        lastClickAt_ = clickAt;

        if (selectedSettingRow_ != row) {
            discardPendingEdit();
            setSelectedSettingRow(row);
            statusMessage_.clear();
        }

        if (isControlPage()) {
            if (isDoubleClick && row < controlCount()) {
                beginRebindForRow(row);
            }
            return;
        }

        const auto controlType = hud.lines[static_cast<std::size_t>(line)].controlType;
        const bool disabled = isSettingRowDisabled(row);
        const auto widgets = layout.lineWidgets(
            static_cast<std::size_t>(visible),
            hud.lines[static_cast<std::size_t>(line)]);

        if (disabled) {
            statusMessage_ = disabledReasonForRow(row);
            return;
        }

        if (controlType == OverlayRenderer::PauseMenuControlType::Toggle) {
            if (widgets.hasCheckbox && widgets.checkbox.contains(xPx, yPx)) {
                adjustSelectedSetting(1, false);
            }
            return;
        }

        if (controlType == OverlayRenderer::PauseMenuControlType::Choice ||
            controlType == OverlayRenderer::PauseMenuControlType::Numeric)
        {
            if (widgets.hasLeftArrow && widgets.leftArrow.contains(xPx, yPx)) {
                adjustSelectedSetting(-1, false);
            } else if (widgets.hasRightArrow && widgets.rightArrow.contains(xPx, yPx)) {
                adjustSelectedSetting(1, false);
            }
            return;
        }
    }

    void PauseMenuController::updateContinuousInput(
        GLFWwindow* window,
        const input::ControlBindings& controls)
    {
        const bool controlsDirty = controls != input::ControlBindings{};

        if (!open_ || window == nullptr || awaitingRebind_) {
            return;
        }

        const double now = glfwGetTime();
        constexpr double kRepeatDelay = 0.35;
        constexpr double kRepeatInterval = 0.08;

        const auto handleRepeat = [&](const int key,
                                      bool& held,
                                      double& nextRepeatAt,
                                      const auto& onRepeat) {
            if (const bool down = glfwGetKey(window, key) == GLFW_PRESS; !down) {
                held = false;
                nextRepeatAt = 0.0;
                return;
            }

            if (!held) {
                held = true;
                nextRepeatAt = now + kRepeatDelay;
                return;
            }

            if (now >= nextRepeatAt) {
                nextRepeatAt = now + kRepeatInterval;
                onRepeat();
            }
        };

        handleRepeat(GLFW_KEY_UP, upHeld_, upNextRepeatAt_, [&] { moveVerticalFocus(-1, controlsDirty); });
        handleRepeat(GLFW_KEY_DOWN, downHeld_, downNextRepeatAt_, [&] { moveVerticalFocus(1, controlsDirty); });
        handleRepeat(GLFW_KEY_LEFT, leftHeld_, leftNextRepeatAt_, [&] { moveHorizontalFocus(-1); });
        handleRepeat(GLFW_KEY_RIGHT, rightHeld_, rightNextRepeatAt_, [&] { moveHorizontalFocus(1); });

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS &&
            (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        {
            resetAllSettings(window);
        }
    }
} // namespace ui

namespace ui::pause_menu_shared {
    using overlay_renderer::draw_shared::fitScaleForWidth;
    using overlay_renderer::draw_shared::measureMaxLinePx;

    Rect PauseMenuLayout::lineRect(const std::size_t visibleIndex) const {
        const float lineY = linesStartY + static_cast<float>(visibleIndex) * rowStep;
        const bool showScrollbar = totalLines > visibleLines;
        const float scrollbarReserve = showScrollbar ? 22.0f * menuScale : 0.0f;
        return Rect{
            .x0 = sectionX0 + 10.0f * menuScale,
            .y0 = lineY - 2.0f * menuScale,
            .x1 = sectionX1 - 10.0f * menuScale - scrollbarReserve,
            .y1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale,
        };
    }

    const Rect& PauseMenuLayout::actionRect(const OverlayRenderer::PauseMenuAction action) const {
        return actionRects[static_cast<std::size_t>(action)];
    }

    PauseMenuLayout::LineWidgets PauseMenuLayout::lineWidgets(
        const std::size_t visibleIndex,
        const OverlayRenderer::PauseMenuHudLine& line) const
    {
        LineWidgets widgets{};
        widgets.line = lineRect(visibleIndex);
        if (line.header) {
            return widgets;
        }

        const float buttonY0 = widgets.line.y0;
        const float buttonY1 = widgets.line.y1;
        const float rightX1 = widgets.line.x1 - 10.0f * menuScale;
        const float valueScalePx = rowScalePx * 0.88f;

        switch (line.controlType) {
            case OverlayRenderer::PauseMenuControlType::Toggle: {
                const float valueW = 220.0f * menuScale;
                const float valueX1 = rightX1 - 24.0f * menuScale - 6.0f * menuScale;
                widgets.value = Rect{
                    .x0 = valueX1 - valueW,
                    .y0 = buttonY0,
                    .x1 = valueX1,
                    .y1 = buttonY1,
                };
                widgets.hasValue = true;

                const float box = std::max(8.0f, (buttonY1 - buttonY0) - 8.0f * menuScale);
                const float checkboxX0 = widgets.value.x0 + 18.0f * menuScale;
                const float checkboxY0 = buttonY0 + ((buttonY1 - buttonY0) - box) * 0.5f;
                widgets.checkbox = Rect{
                    .x0 = checkboxX0,
                    .y0 = checkboxY0,
                    .x1 = checkboxX0 + box,
                    .y1 = checkboxY0 + box,
                };
                widgets.hasCheckbox = true;
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Choice:
            case OverlayRenderer::PauseMenuControlType::Numeric: {
                const float arrowW = 24.0f * menuScale;
                const float gap = 6.0f * menuScale;
                const float valueW = 220.0f * menuScale;
                widgets.rightArrow = Rect{
                    .x0 = rightX1 - arrowW,
                    .y0 = buttonY0,
                    .x1 = rightX1,
                    .y1 = buttonY1,
                };
                widgets.hasRightArrow = true;
                widgets.value = Rect{
                    .x0 = widgets.rightArrow.x0 - gap - valueW,
                    .y0 = buttonY0,
                    .x1 = widgets.rightArrow.x0 - gap,
                    .y1 = buttonY1,
                };
                widgets.hasValue = true;
                widgets.leftArrow = Rect{
                    .x0 = widgets.value.x0 - gap - arrowW,
                    .y0 = buttonY0,
                    .x1 = widgets.value.x0 - gap,
                    .y1 = buttonY1,
                };
                widgets.hasLeftArrow = true;
                if (line.controlType == OverlayRenderer::PauseMenuControlType::Numeric && line.showSlider) {
                    const float sliderY1 = buttonY1 - 3.0f * menuScale;
                    widgets.slider = Rect{
                        .x0 = widgets.value.x0 + 6.0f * menuScale,
                        .y0 = sliderY1 - 6.0f * menuScale,
                        .x1 = widgets.value.x1 - 6.0f * menuScale,
                        .y1 = sliderY1,
                    };
                    widgets.hasSlider = true;
                }
                break;
            }
            case OverlayRenderer::PauseMenuControlType::Rebind:
            case OverlayRenderer::PauseMenuControlType::Action: {
                if (!line.valueText.empty()) {
                    const float valueW = std::clamp(
                        measureMaxLinePx(line.valueText, valueScalePx) + 18.0f * menuScale,
                        88.0f * menuScale,
                        150.0f * menuScale);
                    widgets.value = Rect{
                        .x0 = rightX1 - valueW,
                        .y0 = buttonY0,
                        .x1 = rightX1,
                        .y1 = buttonY1,
                    };
                    widgets.hasValue = true;
                }
                break;
            }
            case OverlayRenderer::PauseMenuControlType::None:
            default: {
                if (!line.valueText.empty()) {
                    const float valueW = measureMaxLinePx(line.valueText, valueScalePx);
                    widgets.value = Rect{
                        .x0 = rightX1 - 2.0f * menuScale - valueW,
                        .y0 = buttonY0,
                        .x1 = rightX1 - 2.0f * menuScale,
                        .y1 = buttonY1,
                    };
                    widgets.hasValue = true;
                }
                break;
            }
        }

        return widgets;
    }

    std::string formatSpeedMultiplier(const double value) {
        char buffer[32];
        if (value >= 100.0) {
            std::snprintf(buffer, sizeof(buffer), "%.0fX", value);
        } else if (value >= 10.0) {
            std::snprintf(buffer, sizeof(buffer), "%.1fX", value);
        } else if (value >= 1.0) {
            std::snprintf(buffer, sizeof(buffer), "%.2fX", value);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.4fX", value);
        }
        return buffer;
    }

    std::string formatGravityMultiplier(const double value) {
        char buffer[32];
        const double multiplier = value / kDefaultGravityStrength;
        if (multiplier >= 10.0) {
            std::snprintf(buffer, sizeof(buffer), "%.0fX", multiplier);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.2fX", multiplier);
        }
        return buffer;
    }

    std::string formatRestitutionPercent(const double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0, 1.0) * 100.0);
        return buffer;
    }

    PauseMenuLayout buildPauseMenuLayout(
        const float width,
        const float height,
        const float uiScale,
        const OverlayRenderer::PauseMenuHud& hud)
    {
        PauseMenuLayout layout{};
        layout.width = width;
        layout.height = height;
        layout.menuScale = std::clamp(uiScale, 0.75f, 1.50f);
        layout.baseScalePx = kBaseScalePx * layout.menuScale;
        layout.tabsScalePx = layout.baseScalePx;
        layout.actionScalePx = layout.baseScalePx * 0.98f;
        layout.cardX0 = (width - std::min(width * 0.82f, 1240.0f)) * 0.5f;
        layout.cardY0 = (height - std::min(height * 0.86f, 920.0f)) * 0.5f;
        layout.cardX1 = layout.cardX0 + std::min(width * 0.82f, 1240.0f);
        layout.cardY1 = layout.cardY0 + std::min(height * 0.86f, 920.0f);

        const float statusSlotY = layout.cardY0 + 96.0f * layout.menuScale;
        const float statusReservedScalePx = layout.baseScalePx * 0.95f;
        const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
        const float tabsSlotY = statusSlotY + statusReservedHeight + 16.0f * layout.menuScale;
        const float tabPadX = 12.0f * layout.menuScale;
        const float tabPadY = 7.0f * layout.menuScale;
        const float tabGap = 12.0f * layout.menuScale;
        const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * layout.tabsScalePx + tabPadY * 2.0f;
        float tabX = layout.cardX0 + 18.0f * layout.menuScale;
        for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
            const float tabWidth = measureMaxLinePx(kPageTabLabels[i], layout.tabsScalePx) + tabPadX * 2.0f;
            layout.tabRects[i] = Rect{tabX, tabsSlotY, tabX + tabWidth, tabsSlotY + tabHeight};
            tabX += tabWidth + tabGap;
        }

        const float buttonPadX = 12.0f * layout.menuScale;
        const float exitWidth = measureMaxLinePx("EXIT TO HOME", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.actionRects[actionIndex(PauseAction::Exit)] = Rect{
            .x0 = layout.cardX1 - 18.0f * layout.menuScale - exitWidth,
            .y0 = tabsSlotY,
            .x1 = layout.cardX1 - 18.0f * layout.menuScale,
            .y1 = tabsSlotY + tabHeight,
        };

        const float closeWidth = measureMaxLinePx("X", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.actionRects[actionIndex(PauseAction::Close)] = Rect{
            .x0 = layout.cardX1 - 18.0f * layout.menuScale - closeWidth,
            .y0 = layout.cardY0 + 18.0f * layout.menuScale,
            .x1 = layout.cardX1 - 18.0f * layout.menuScale,
            .y1 = layout.cardY0 + 18.0f * layout.menuScale + tabHeight,
        };

        const float resetWidth = measureMaxLinePx("R", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.actionRects[actionIndex(PauseAction::ResetIcon)] = Rect{
            .x0 = layout.cardX0 + 18.0f * layout.menuScale,
            .y0 = layout.cardY0 + 18.0f * layout.menuScale,
            .x1 = layout.cardX0 + 18.0f * layout.menuScale + resetWidth,
            .y1 = layout.cardY0 + 18.0f * layout.menuScale + tabHeight,
        };

        const float footerReservedH = (static_cast<float>(kFontH) + 3.0f) * (layout.baseScalePx * 0.92f) + 18.0f * layout.menuScale;
        layout.settingsY0 = std::max(layout.cardY0 + 170.0f * layout.menuScale, tabsSlotY + tabHeight + 16.0f * layout.menuScale);
        layout.sectionX0 = layout.cardX0 + 26.0f * layout.menuScale;
        layout.sectionX1 = layout.cardX1 - 26.0f * layout.menuScale;
        layout.settingsY1 = layout.cardY1 - 20.0f * layout.menuScale - footerReservedH;
        if (layout.settingsY1 - layout.settingsY0 < 220.0f) {
            layout.settingsY0 = layout.settingsY1 - 220.0f;
        }

        float maxRowWidth = 0.0f;
        float maxControlReserve = 0.0f;
        for (const auto& line : hud.lines) {
            maxRowWidth = std::max(maxRowWidth, measureMaxLinePx(line.text, 1.0f));
            if (line.header) {
                continue;
            }
            switch (line.controlType) {
                case OverlayRenderer::PauseMenuControlType::Toggle:
                    maxControlReserve = std::max(maxControlReserve, 128.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Choice:
                case OverlayRenderer::PauseMenuControlType::Numeric:
                    maxControlReserve = std::max(maxControlReserve, 248.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Rebind:
                    maxControlReserve = std::max(maxControlReserve, 240.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Action:
                    maxControlReserve = std::max(maxControlReserve, 120.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::None:
                default:
                    break;
            }
        }

        layout.rowScalePx = kSettingsScalePx * layout.menuScale * 1.8f;
        if (const float rowAreaWidth = (layout.sectionX1 - layout.sectionX0) - 28.0f * layout.menuScale - maxControlReserve; maxRowWidth > 0.0f && rowAreaWidth > 0.0f) {
            layout.rowScalePx = std::min(layout.rowScalePx, rowAreaWidth / maxRowWidth);
        }

        layout.sectionHeaderScalePx = fitScaleForWidth("SETTINGS", layout.rowScalePx * 1.18f, (layout.sectionX1 - layout.sectionX0) - 28.0f);
        const float sectionHeaderY = layout.settingsY0 + 10.0f * layout.menuScale;
        layout.linesStartY =
            sectionHeaderY + (static_cast<float>(kFontH) + 3.0f) * layout.sectionHeaderScalePx + 9.0f * layout.menuScale;
        layout.rowStep = (static_cast<float>(kFontH) + 4.0f) * layout.rowScalePx;
        layout.actionButtonH = (static_cast<float>(kFontH) + 2.0f) * layout.actionScalePx + 8.0f * layout.menuScale * 2.0f;
        const float actionReservedH = layout.actionButtonH + 20.0f * layout.menuScale;
        layout.contentY1 = layout.settingsY1 - 10.0f * layout.menuScale - actionReservedH;
        const float maxLines = std::floor((layout.contentY1 - layout.linesStartY) / layout.rowStep);
        layout.totalLines = hud.lines.size();
        layout.visibleLines = std::min<std::size_t>(layout.totalLines, static_cast<std::size_t>(std::max(0.0f, maxLines)));
        layout.firstLine = 0;
        if (layout.visibleLines > 0 && layout.totalLines > layout.visibleLines) {
            const std::size_t maxFirst = layout.totalLines - layout.visibleLines;
            layout.firstLine = std::min<std::size_t>(static_cast<std::size_t>(std::max(0, hud.firstVisibleLineIndex)), maxFirst);
            if (hud.selectedSettingLineIndex >= 0) {
                const auto selected = static_cast<std::size_t>(
                    std::clamp(hud.selectedSettingLineIndex, 0, static_cast<int>(layout.totalLines - 1)));
                if (selected < layout.firstLine) {
                    layout.firstLine = selected;
                } else if (selected >= layout.firstLine + layout.visibleLines) {
                    layout.firstLine = selected - layout.visibleLines + 1;
                }
            }
            layout.firstLine = std::min(layout.firstLine, maxFirst);
        }

        layout.popupRect = Rect{
            .x0 = layout.cardX0 + ((layout.cardX1 - layout.cardX0) - 320.0f * layout.menuScale) * 0.5f,
            .y0 = layout.cardY0 + ((layout.cardY1 - layout.cardY0) - 120.0f * layout.menuScale) * 0.5f,
            .x1 = 0.0f,
            .y1 = 0.0f,
        };
        layout.popupRect.x1 = layout.popupRect.x0 + 320.0f * layout.menuScale;
        layout.popupRect.y1 = layout.popupRect.y0 + 120.0f * layout.menuScale;

        if (hud.actions[actionIndex(PauseAction::Apply)].visible) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("APPLY CHANGES", layout.actionScalePx) + actionPadX * 2.0f;
            layout.actionRects[actionIndex(PauseAction::Apply)] = Rect{
                .x0 = layout.sectionX1 - 12.0f * layout.menuScale - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = layout.sectionX1 - 12.0f * layout.menuScale,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        if (hud.actions[actionIndex(PauseAction::ResetWorld)].visible) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("RESET WORLD", layout.actionScalePx) + actionPadX * 2.0f;
            float actionX1 = layout.sectionX1 - 12.0f * layout.menuScale;
            if (hud.actions[actionIndex(PauseAction::Apply)].visible) {
                const float applyW = measureMaxLinePx("APPLY CHANGES", layout.actionScalePx) + actionPadX * 2.0f;
                actionX1 -= applyW + 12.0f * layout.menuScale;
            }
            layout.actionRects[actionIndex(PauseAction::ResetWorld)] = Rect{
                .x0 = actionX1 - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = actionX1,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        if (hud.actions[actionIndex(PauseAction::ResetControls)].visible) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("RESET CONTROLS", layout.actionScalePx) + actionPadX * 2.0f;
            layout.actionRects[actionIndex(PauseAction::ResetControls)] = Rect{
                .x0 = layout.sectionX1 - 12.0f * layout.menuScale - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = layout.sectionX1 - 12.0f * layout.menuScale,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        return layout;
    }

    PopupButtonsLayout buildPopupButtonsLayout(
        const PauseMenuLayout& layout,
        const OverlayRenderer::PauseMenuHud& hud)
    {
        PopupButtonsLayout popup{};
        popup.singleAcknowledge = hud.resetConfirmSingleAcknowledge;
        popup.buttonScalePx = layout.baseScalePx * 0.92f;
        popup.buttonPadX = 16.0f * layout.menuScale;
        popup.buttonPadY = 8.0f * layout.menuScale;
        popup.yesLabel = hud.resetConfirmYesLabel.empty() ? "RESET" : hud.resetConfirmYesLabel;
        popup.noLabel = hud.resetConfirmNoLabel.empty() ? "CANCEL" : hud.resetConfirmNoLabel;

        const float yesWidth = measureMaxLinePx(popup.yesLabel, popup.buttonScalePx) + popup.buttonPadX * 2.0f;
        const float buttonHeight =
            (static_cast<float>(kFontH) + 2.0f) * popup.buttonScalePx + popup.buttonPadY * 2.0f;
        const float buttonsY1 = layout.popupRect.y1 - 14.0f * layout.menuScale;
        const float buttonsY0 = buttonsY1 - buttonHeight;

        if (popup.singleAcknowledge) {
            popup.yesButton = Rect{
                .x0 = layout.popupRect.x0 + ((layout.popupRect.x1 - layout.popupRect.x0) - yesWidth) * 0.5f,
                .y0 = buttonsY0,
                .x1 = 0.0f,
                .y1 = buttonsY1,
            };
            popup.yesButton.x1 = popup.yesButton.x0 + yesWidth;
            return popup;
        }

        const float noWidth = measureMaxLinePx(popup.noLabel, popup.buttonScalePx) + popup.buttonPadX * 2.0f;
        popup.noButton = Rect{
            .x0 = layout.popupRect.x1 - 18.0f * layout.menuScale - noWidth,
            .y0 = buttonsY0,
            .x1 = layout.popupRect.x1 - 18.0f * layout.menuScale,
            .y1 = buttonsY1,
        };
        popup.yesButton = Rect{
            .x0 = popup.noButton.x0 - 10.0f * layout.menuScale - yesWidth,
            .y0 = buttonsY0,
            .x1 = popup.noButton.x0 - 10.0f * layout.menuScale,
            .y1 = buttonsY1,
        };
        return popup;
    }
} // namespace ui::pause_menu_shared

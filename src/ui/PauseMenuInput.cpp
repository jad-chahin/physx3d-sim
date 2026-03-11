#include "ui/PauseMenuController.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ui {
    namespace {
        constexpr int kFontH = 7;
        constexpr int kAdvance = 6;

        float measureMaxLinePx(const std::string& s, const float scalePx) {
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

        float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
            const float unitWidth = measureMaxLinePx(s, 1.0f);
            if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) {
                return preferredScalePx;
            }
            return std::min(preferredScalePx, maxWidthPx / unitWidth);
        }
    } // namespace

    void PauseMenuController::updateEscapeState(
        GLFWwindow* window,
        bool& mouseCaptured,
        bool& firstMouse,
        double& lastTime,
        double& accumulator,
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

            const bool triggeredByResume = resumeRequested_;
            open_ = !open_;
            if (triggeredByResume) {
                open_ = false;
            }
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            ignoreNextRebindMousePress_ = false;
            resumeRequested_ = false;
            hoveredSettingRow_ = -1;
            hoveredDisplayApplyAction_ = false;
            hoveredResetControlsAction_ = false;
            hoveredResetIcon_ = false;
            hoveredResetConfirmYes_ = false;
            hoveredResetConfirmNo_ = false;
            confirmingResetSettings_ = false;
            discardPendingEdit();
            statusMessage_.clear();
            upHeld_ = false;
            downHeld_ = false;
            leftHeld_ = false;
            rightHeld_ = false;

            if (open_) {
                const int count = settingCount();
                if (count > 0 && selectedSettingRow_ >= 0) {
                    setSelectedSettingRow(std::clamp(selectedSettingRow_, 0, count - 1));
                } else {
                    setSelectedSettingRow(-1);
                }

                mouseCaptured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                mouseCaptured = true;
                firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                lastTime = glfwGetTime();
                accumulator = 0.0;
                for (auto& body : bodies) {
                    body.prevPosition = body.position;
                }
            }
        }

        escWasDown_ = escDown;
        backWasDown_ = backDown;
    }

    void PauseMenuController::handlePressedKey(
        GLFWwindow* window,
        int pressedKey,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
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
                selectPrev();
                return;
            case GLFW_KEY_DOWN:
                selectNext();
                return;
            case GLFW_KEY_LEFT:
                if (selectedSettingRow_ >= 0 && !isControlPage()) {
                    adjustSelectedSetting(-1);
                }
                return;
            case GLFW_KEY_RIGHT:
                if (selectedSettingRow_ >= 0 && !isControlPage()) {
                    adjustSelectedSetting(1);
                }
                return;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
            case GLFW_KEY_SPACE:
                if (selectedSettingRow_ < 0) {
                    return;
                }
                if (isControlPage()) {
                    beginRebindForRow(selectedSettingRow_);
                } else {
                    adjustSelectedSetting(1);
                }
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
            hoveredSettingRow_ = -1;
            hoveredDisplayApplyAction_ = false;
            hoveredResetControlsAction_ = false;
            hoveredResetIcon_ = false;
            hoveredResetConfirmYes_ = false;
            hoveredResetConfirmNo_ = false;
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
        const float menuScale = std::clamp(uiScale(), 0.80f, 1.40f);
        const float baseScalePx = 2.0f * menuScale;
        const float cardW = std::min(w * 0.82f, 1240.0f);
        const float cardH = std::min(h * 0.86f, 920.0f);
        const float cardX0 = (w - cardW) * 0.5f;
        const float cardY0 = (h - cardH) * 0.5f;
        const float cardY1 = cardY0 + cardH;
        const float cardX1 = cardX0 + cardW;

        const float statusSlotY = cardY0 + 96.0f * menuScale;
        const float statusReservedScalePx = baseScalePx * 0.95f;
        const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
        const float tabsSlotY = statusSlotY + statusReservedHeight + 16.0f * menuScale;

        constexpr std::array<SettingsPage, 5> kPageOrder{
            SettingsPage::Display, SettingsPage::Simulation, SettingsPage::Camera, SettingsPage::Interface, SettingsPage::Controls,
        };
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

        const OverlayRenderer::PauseMenuHud hud = buildHud(controls, false);
        if (hud.lines.empty()) {
            hoveredSettingRow_ = -1;
            return;
        }

        constexpr float kSettingsScalePx = 2.10f;
        const float footerReservedH = (static_cast<float>(kFontH) + 3.0f) * (baseScalePx * 0.92f) + 18.0f * menuScale;
        float settingsY0 = std::max(cardY0 + 170.0f * menuScale, tabsSlotY + tabHeight + 16.0f * menuScale);
        const float sectionX0 = cardX0 + 26.0f * menuScale;
        const float sectionX1 = cardX1 - 26.0f * menuScale;
        const float settingsY1 = cardY1 - 20.0f * menuScale - footerReservedH;
        if (settingsY1 - settingsY0 < 220.0f) {
            settingsY0 = settingsY1 - 220.0f;
        }

        float rowScalePx = kSettingsScalePx * menuScale * 1.8f;
        float maxRowWidth = 0.0f;
        float maxControlReserve = 0.0f;
        for (const auto& line : hud.lines) {
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
        const float sectionHeaderScalePx =
            fitScaleForWidth(settingsTitle, rowScalePx * 1.18f, (sectionX1 - sectionX0) - 28.0f);
        const float sectionHeaderY = settingsY0 + 10.0f * menuScale;
        const float linesStartY =
            sectionHeaderY + (static_cast<float>(kFontH) + 3.0f) * sectionHeaderScalePx + 9.0f * menuScale;
        const float rowStep = (static_cast<float>(kFontH) + 4.0f) * rowScalePx;
        const float settingsBottomPad = 10.0f * menuScale;
        const float actionReservedH =
            (hud.showDisplayApplyAction || hud.showResetControlsAction) ? 44.0f * menuScale : 0.0f;
        const float contentY1 = settingsY1 - settingsBottomPad - actionReservedH;
        const float maxLines = std::floor((contentY1 - linesStartY) / rowStep);

        const std::size_t totalLines = hud.lines.size();
        const std::size_t visibleLines = std::min<std::size_t>(
            totalLines,
            static_cast<std::size_t>(std::max(0.0f, maxLines)));

        std::size_t firstLine = 0;
        if (visibleLines > 0 && totalLines > visibleLines) {
            const int clampedSelected = std::clamp(hud.selectedSettingLineIndex, 0, static_cast<int>(totalLines - 1));
            const auto selected = static_cast<std::size_t>(clampedSelected);
            if (selected >= visibleLines) {
                firstLine = selected - visibleLines + 1;
            }
            const std::size_t maxFirst = totalLines - visibleLines;
            if (firstLine > maxFirst) {
                firstLine = maxFirst;
            }
        }

        const int count = settingCount();
        int firstSelectableLine = -1;
        for (std::size_t i = 0; i < hud.lines.size(); ++i) {
            if (!hud.lines[i].header) {
                firstSelectableLine = static_cast<int>(i);
                break;
            }
        }
        if (firstSelectableLine < 0) {
            hoveredSettingRow_ = -1;
            return;
        }
        int hoveredLineIndex = -1;
        int hoveredRowIndex = -1;
        float hoveredLineX1 = 0.0f;
        float hoveredLineY0 = 0.0f;
        float hoveredLineY1 = 0.0f;
        for (std::size_t i = 0; i < visibleLines; ++i) {
            const std::size_t lineIdx = firstLine + i;
            const float lineY = linesStartY + static_cast<float>(i) * rowStep;
            const float lineX0 = sectionX0 + 10.0f * menuScale;
            const float lineX1 = sectionX1 - 10.0f * menuScale;
            const float lineY0 = lineY - 2.0f * menuScale;
            const float lineY1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale;

            if (xPx < lineX0 || xPx > lineX1 || yPx < lineY0 || yPx > lineY1) {
                continue;
            }

            if (hud.lines[lineIdx].header) {
                hoveredSettingRow_ = -1;
                break;
            }

            const int rowIndex = static_cast<int>(lineIdx) - firstSelectableLine;
            if (rowIndex < 0 || rowIndex >= count) {
                break;
            }
            hoveredLineIndex = static_cast<int>(lineIdx);
            hoveredRowIndex = rowIndex;
            hoveredLineX1 = lineX1;
            hoveredLineY0 = lineY0;
            hoveredLineY1 = lineY1;
            break;
        }

        hoveredSettingRow_ = hoveredRowIndex;
        hoveredDisplayApplyAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;

        if (settingsPage_ == SettingsPage::Display && pendingDisplayChanges_) {
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
            hoveredDisplayApplyAction_ =
                xPx >= actionX0 && xPx <= actionX1 && yPx >= actionY0 && yPx <= actionY1;
        }
        if (hud.showResetControlsAction) {
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
            hoveredResetControlsAction_ =
                xPx >= actionX0 && xPx <= actionX1 && yPx >= actionY0 && yPx <= actionY1;
        }

        if (awaitingRebind_) {
            if (pressedMouseBindingCode == GLFW_KEY_UNKNOWN) {
                return;
            }
            if (ignoreNextRebindMousePress_) {
                ignoreNextRebindMousePress_ = false;
                return;
            }
            if (hoveredRowIndex != selectedSettingRow_)
            {
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
            const int wheelDirection = scrollDeltaY > 0.0f ? 1 : -1;
            const int wheelSteps = std::min(4, std::max(1, static_cast<int>(std::fabs(scrollDeltaY))));
            if (hoveredRowIndex >= 0) {
                if (selectedSettingRow_ != hoveredRowIndex) {
                    if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                        discardPendingEdit();
                    }
                    setSelectedSettingRow(hoveredRowIndex);
                }
                const auto controlType = controlTypeForRow(hoveredRowIndex);
                if (!isControlPage() &&
                    !isSettingRowDisabled(hoveredRowIndex) &&
                    (controlType == OverlayRenderer::PauseMenuControlType::Numeric ||
                     controlType == OverlayRenderer::PauseMenuControlType::Choice))
                {
                    for (int i = 0; i < wheelSteps; ++i) {
                        adjustSelectedSetting(wheelDirection);
                    }
                    return;
                }
            }
            for (int i = 0; i < wheelSteps; ++i) {
                if (wheelDirection > 0) {
                    selectPrev();
                } else {
                    selectNext();
                }
            }
        }

        if (!clickPressed) {
            return;
        }

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

        if (xPx >= closeX0 && xPx <= closeX1 && yPx >= closeY0 && yPx <= closeY1) {
            resumeRequested_ = true;
            return;
        }
        if (xPx >= exitX0 && xPx <= exitX1 && yPx >= exitY0 && yPx <= exitY1) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        const float tabXStart = cardX0 + 18.0f * menuScale;
        float tabX = tabXStart;
        for (std::size_t i = 0; i < kPageOrder.size(); ++i) {
            const float x0 = tabX;
            const float x1 = x0 + tabWidths[i];
            const float y0 = tabsSlotY;
            const float y1 = tabsSlotY + tabHeight;
            if (xPx >= x0 && xPx <= x1 && yPx >= y0 && yPx <= y1) {
                switchToPage(kPageOrder[i]);
                return;
            }
            tabX = x1 + tabGap;
        }

        hoveredResetIcon_ =
            hud.showResetIcon && xPx >= resetX0 && xPx <= resetX1 && yPx >= resetY0 && yPx <= resetY1;

        if (!confirmingResetSettings_ && hoveredResetIcon_) {
            if (clickPressed) {
                confirmingResetSettings_ = true;
            }
            return;
        }

        if (confirmingResetSettings_) {
            const float popupW = 320.0f * menuScale;
            const float popupH = 120.0f * menuScale;
            const float popupX0 = cardX0 + (cardW - popupW) * 0.5f;
            const float popupY0 = cardY0 + (cardH - popupH) * 0.5f;
            const float popupX1 = popupX0 + popupW;
            const float popupY1 = popupY0 + popupH;
            const float buttonScalePx = baseScalePx * 0.92f;
            const float buttonPadX1 = 16.0f * menuScale;
            const float buttonPadY = 8.0f * menuScale;
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
            hoveredResetConfirmYes_ =
                xPx >= yesX0 && xPx <= yesX1 && yPx >= buttonsY0 && yPx <= buttonsY1;
            hoveredResetConfirmNo_ =
                xPx >= noX0 && xPx <= noX1 && yPx >= buttonsY0 && yPx <= buttonsY1;

            if (clickPressed) {
                if (hoveredResetConfirmYes_) {
                    resetAllSettings(window);
                    confirmingResetSettings_ = false;
                } else if (hoveredResetConfirmNo_ ||
                           !(xPx >= popupX0 && xPx <= popupX1 && yPx >= popupY0 && yPx <= popupY1))
                {
                    confirmingResetSettings_ = false;
                }
            }
            return;
        }

        if (hoveredRowIndex < 0 || hoveredLineIndex < 0) {
            if (hoveredDisplayApplyAction_) {
                if (clickPressed) {
                    commitSelectedSetting(window);
                }
                return;
            }
            if (hoveredResetControlsAction_) {
                if (clickPressed) {
                    controls = input::ControlBindings{};
                    input::saveControlBindings(controls, controlsConfigPath);
                    markAppliedSelection();
                    statusMessage_.clear();
                }
                return;
            }
            if (selectedSettingRow_ >= 0 &&
                xPx >= cardX0 && xPx <= cardX1 &&
                yPx >= settingsY0 && yPx <= settingsY1)
            {
                if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                    discardPendingEdit();
                }
                setSelectedSettingRow(-1);
                statusMessage_.clear();
            }
            return;
        }

        if (selectedSettingRow_ != hoveredRowIndex) {
            if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                discardPendingEdit();
            }
            setSelectedSettingRow(hoveredRowIndex);
            statusMessage_.clear();
        }

        if (isControlPage()) {
            return;
        }

        const auto controlType = controlTypeForRow(hoveredRowIndex);
        const bool disabled = isSettingRowDisabled(hoveredRowIndex);

        if (disabled) {
            statusMessage_ = disabledReasonForRow(hoveredRowIndex);
            return;
        }

        if (controlType == OverlayRenderer::PauseMenuControlType::Toggle) {
            adjustSelectedSetting(1);
            return;
        }

        if (controlType == OverlayRenderer::PauseMenuControlType::Choice ||
            controlType == OverlayRenderer::PauseMenuControlType::Numeric)
        {
            const float arrowW = 24.0f * menuScale;
            const float gap = 6.0f * menuScale;
            const float valueW = 220.0f * menuScale;
            const float rightX1 = hoveredLineX1 - 10.0f * menuScale;
            const float rightArrowX0 = rightX1 - arrowW;
            const float valueX1 = rightArrowX0 - gap;
            const float valueX0 = valueX1 - valueW;
            const float leftArrowX1 = valueX0 - gap;
            const float leftArrowX0 = leftArrowX1 - arrowW;

            const bool clickedLeftArrow =
                xPx >= leftArrowX0 && xPx <= leftArrowX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1;
            const bool clickedRightArrow =
                xPx >= rightArrowX0 && xPx <= rightX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1;
            const bool clickedValue =
                xPx >= valueX0 && xPx <= valueX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1;

            if (clickedLeftArrow) {
                adjustSelectedSetting(-1);
            } else if (clickedRightArrow) {
                adjustSelectedSetting(1);
            } else if (clickedValue) {
                adjustSelectedSetting(xPx < (valueX0 + valueX1) * 0.5f ? -1 : 1);
            }
            return;
        }
    }

    void PauseMenuController::updateContinuousInput(
        GLFWwindow* window,
        const input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        (void)controls;
        (void)controlsConfigPath;

        if (!open_ || window == nullptr || awaitingRebind_) {
            return;
        }

        const double now = glfwGetTime();
        constexpr double kRepeatDelay = 0.35;
        constexpr double kRepeatInterval = 0.08;

        const auto handleRepeat = [&](const int key,
                                      bool& held,
                                      double& nextRepeatAt,
                                      const auto& onInitial,
                                      const auto& onRepeat) {
            const bool down = glfwGetKey(window, key) == GLFW_PRESS;
            if (!down) {
                held = false;
                nextRepeatAt = 0.0;
                return;
            }

            if (!held) {
                held = true;
                nextRepeatAt = now + kRepeatDelay;
                onInitial();
                return;
            }

            if (now >= nextRepeatAt) {
                nextRepeatAt = now + kRepeatInterval;
                onRepeat();
            }
        };

        handleRepeat(GLFW_KEY_UP, upHeld_, upNextRepeatAt_, [&] { selectPrev(); }, [&] { selectPrev(); });
        handleRepeat(GLFW_KEY_DOWN, downHeld_, downNextRepeatAt_, [&] { selectNext(); }, [&] { selectNext(); });
        handleRepeat(
            GLFW_KEY_LEFT,
            leftHeld_,
            leftNextRepeatAt_,
            [&] {
                if (selectedSettingRow_ >= 0 && !isControlPage() && supportsContinuousAdjust(selectedSettingRow_)) {
                    adjustSelectedSetting(-1);
                }
            },
            [&] {
                if (selectedSettingRow_ >= 0 && !isControlPage() && supportsContinuousAdjust(selectedSettingRow_)) {
                    adjustSelectedSetting(-1);
                }
            });
        handleRepeat(
            GLFW_KEY_RIGHT,
            rightHeld_,
            rightNextRepeatAt_,
            [&] {
                if (selectedSettingRow_ >= 0 && !isControlPage() && supportsContinuousAdjust(selectedSettingRow_)) {
                    adjustSelectedSetting(1);
                }
            },
            [&] {
                if (selectedSettingRow_ >= 0 && !isControlPage() && supportsContinuousAdjust(selectedSettingRow_)) {
                    adjustSelectedSetting(1);
                }
            });

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS &&
            (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        {
            resetAllSettings(window);
        }
    }
} // namespace ui

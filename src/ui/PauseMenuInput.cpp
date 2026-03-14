#include "ui/PauseMenuController.h"
#include "ui/PauseMenuShared.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace ui {
    using namespace pause_menu_shared;

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
            hoveredSettingRow_ = -1;
            hoveredDisplayApplyAction_ = false;
            hoveredResetWorldAction_ = false;
            hoveredResetControlsAction_ = false;
            hoveredResetIcon_ = false;
            hoveredResetConfirmYes_ = false;
            hoveredResetConfirmNo_ = false;
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
        controlsDirty_ = hasControlChanges(controls);
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
                selectLeft(window, controls, controlsConfigPath);
                return;
            case GLFW_KEY_RIGHT:
                selectRight(window, controls, controlsConfigPath);
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
        controlsDirty_ = hasControlChanges(controls);
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
            hoveredResetWorldAction_ = false;
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
        constexpr std::array<SettingsPage, 5> kPageOrder{
            SettingsPage::Display, SettingsPage::Simulation, SettingsPage::Camera, SettingsPage::Interface, SettingsPage::Controls,
        };

        const OverlayRenderer::PauseMenuHud hud = buildHud(controls, false);
        if (hud.lines.empty()) {
            hoveredSettingRow_ = -1;
            return;
        }
        const PauseMenuLayout layout = buildPauseMenuLayout(w, h, uiScale(), hud);
        const float menuScale = layout.menuScale;
        const float cardX0 = layout.cardX0;
        const float cardX1 = layout.cardX1;
        const float settingsY0 = layout.settingsY0;
        const float settingsY1 = layout.settingsY1;
        const std::size_t totalLines = layout.totalLines;
        const std::size_t visibleLines = layout.visibleLines;
        const std::size_t firstLine = layout.firstLine;

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
            const Rect lineRect = layout.lineRect(i);

            if (!lineRect.contains(xPx, yPx)) {
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
            hoveredLineX1 = lineRect.x1;
            hoveredLineY0 = lineRect.y0;
            hoveredLineY1 = lineRect.y1;
            break;
        }

        hoveredSettingRow_ = hoveredRowIndex;
        hoveredDisplayApplyAction_ = false;
        hoveredResetWorldAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;

        if (hud.showApplyAction) {
            hoveredDisplayApplyAction_ = layout.applyAction.contains(xPx, yPx);
        }
        if (hud.showResetWorldAction) {
            hoveredResetWorldAction_ = layout.resetWorldAction.contains(xPx, yPx);
        }
        if (hud.showResetControlsAction) {
            hoveredResetControlsAction_ = layout.resetControlsAction.contains(xPx, yPx);
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

        if (!clickPressed) {
            return;
        }

        const double clickAt = glfwGetTime();
        constexpr double kDoubleClickWindowSeconds = 0.35;

        if (layout.closeButton.contains(xPx, yPx)) {
            resumeRequested_ = true;
            return;
        }
        if (layout.exitButton.contains(xPx, yPx)) {
            openPopup(PopupType::ExitToHome, FocusTarget::ExitButton, false);
            return;
        }

        for (std::size_t i = 0; i < kPageOrder.size(); ++i) {
            if (layout.tabRects[i].contains(xPx, yPx)) {
                switchToPage(kPageOrder[i]);
                return;
            }
        }

        hoveredResetIcon_ = hud.showResetIcon && layout.resetIcon.contains(xPx, yPx);

        if (popupType_ != PopupType::ResetSettings && hoveredResetIcon_) {
            if (clickPressed) {
                openPopup(PopupType::ResetSettings, FocusTarget::ResetIcon, false);
            }
            return;
        }

        if (hasOpenPopup()) {
            const float popupX0 = layout.popupRect.x0;
            const float popupY0 = layout.popupRect.y0;
            const float popupX1 = layout.popupRect.x1;
            const float popupY1 = layout.popupRect.y1;
            const float buttonScalePx = layout.baseScalePx * 0.92f;
            const float buttonPadX1 = 16.0f * menuScale;
            const float buttonPadY = 8.0f * menuScale;
            const bool singleAcknowledge = popupUsesSingleAcknowledge();
            const std::string yesLabel = singleAcknowledge
                ? "I UNDERSTAND"
                : (popupType_ == PopupType::ExitToHome ? "EXIT" : "RESET");
            const std::string noLabel = "CANCEL";
            const float yesW = measureMaxLinePx(yesLabel, buttonScalePx) + buttonPadX1 * 2.0f;
            const float buttonH = (static_cast<float>(kFontH) + 2.0f) * buttonScalePx + buttonPadY * 2.0f;
            const float buttonsY1 = popupY1 - 14.0f * menuScale;
            const float buttonsY0 = buttonsY1 - buttonH;
            float noX0 = 0.0f;
            float noX1 = 0.0f;
            float yesX0 = 0.0f;
            float yesX1 = 0.0f;
            if (singleAcknowledge) {
                yesX0 = popupX0 + ((popupX1 - popupX0) - yesW) * 0.5f;
                yesX1 = yesX0 + yesW;
            } else {
                const float noW = measureMaxLinePx(noLabel, buttonScalePx) + buttonPadX1 * 2.0f;
                noX1 = popupX1 - 18.0f * menuScale;
                noX0 = noX1 - noW;
                yesX1 = noX0 - 10.0f * menuScale;
                yesX0 = yesX1 - yesW;
            }
            hoveredResetConfirmYes_ =
                xPx >= yesX0 && xPx <= yesX1 && yPx >= buttonsY0 && yPx <= buttonsY1;
            hoveredResetConfirmNo_ = !singleAcknowledge &&
                xPx >= noX0 && xPx <= noX1 && yPx >= buttonsY0 && yPx <= buttonsY1;

            if (clickPressed) {
                if (hoveredResetConfirmYes_) {
                    if (popupType_ == PopupType::ResetWorld) {
                        resetWorldRequested_ = true;
                        statusMessage_ = "WORLD RESET";
                        closePopup(false);
                    } else if (popupType_ == PopupType::ExitToHome) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                        closePopup(false);
                    } else if (popupType_ == PopupType::EnableDrawPath) {
                        ensureDraftForSelectedRow();
                        draftInterfaceSettings_.drawPath = true;
                        pendingSelectionEdit_ = hasPendingSelectionChanges();
                        if (!pendingSelectionEdit_) {
                            draftSelectionRow_ = -1;
                            draftSettingsPage_ = settingsPage_;
                        }
                        statusMessage_.clear();
                        closePopup();
                    } else {
                        resetAllSettings(window);
                        closePopup(false);
                    }
                } else if (hoveredResetConfirmNo_ ||
                           !(xPx >= popupX0 && xPx <= popupX1 && yPx >= popupY0 && yPx <= popupY1))
                {
                    closePopup();
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
            if (hoveredResetWorldAction_) {
                if (clickPressed) {
                    openPopup(PopupType::ResetWorld, FocusTarget::ResetWorldAction, false);
                }
                return;
            }
            if (hoveredResetControlsAction_) {
                if (clickPressed) {
                    controls = input::ControlBindings{};
                    controlsDirty_ = false;
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
                discardPendingEdit();
                setSelectedSettingRow(-1);
                statusMessage_.clear();
            }
            return;
        }

        const bool isDoubleClick =
            lastClickedPage_ == settingsPage_ &&
            lastClickedRow_ == hoveredRowIndex &&
            lastClickAt_ >= 0.0 &&
            (clickAt - lastClickAt_) <= kDoubleClickWindowSeconds;
        lastClickedPage_ = settingsPage_;
        lastClickedRow_ = hoveredRowIndex;
        lastClickAt_ = clickAt;

        if (selectedSettingRow_ != hoveredRowIndex) {
            discardPendingEdit();
            setSelectedSettingRow(hoveredRowIndex);
            statusMessage_.clear();
        }

        if (isControlPage()) {
            if (isDoubleClick && hoveredRowIndex >= 0 && hoveredRowIndex < controlCount()) {
                beginRebindForRow(hoveredRowIndex);
            }
            return;
        }

        const auto controlType = controlTypeForRow(hoveredRowIndex);
        const bool disabled = isSettingRowDisabled(hoveredRowIndex);

        if (disabled) {
            statusMessage_ = disabledReasonForRow(hoveredRowIndex);
            return;
        }

        if (controlType == OverlayRenderer::PauseMenuControlType::Toggle) {
            const float valueW = 220.0f * menuScale;
            const float rightX1 = hoveredLineX1 - 10.0f * menuScale;
            const float valueX1 = rightX1 - 24.0f * menuScale - 6.0f * menuScale;
            const float toggleX0 = valueX1 - valueW;
            const float box = std::max(8.0f, (hoveredLineY1 - hoveredLineY0) - 8.0f * menuScale);
            const float checkboxX0 = toggleX0 + 18.0f * menuScale;
            const float checkboxX1 = checkboxX0 + box;
            const float checkboxY0 = hoveredLineY0 + ((hoveredLineY1 - hoveredLineY0) - box) * 0.5f;
            const float checkboxY1 = checkboxY0 + box;
            const bool clickedToggle =
                xPx >= checkboxX0 && xPx <= checkboxX1 && yPx >= checkboxY0 && yPx <= checkboxY1;

            if (clickedToggle) {
                adjustSelectedSetting(1);
            }
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
            if (clickedLeftArrow) {
                adjustSelectedSetting(-1);
            } else if (clickedRightArrow) {
                adjustSelectedSetting(1);
            }
            return;
        }
    }

    void PauseMenuController::updateContinuousInput(
        GLFWwindow* window,
        const input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        controlsDirty_ = hasControlChanges(controls);

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
            const bool down = glfwGetKey(window, key) == GLFW_PRESS;
            if (!down) {
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

        handleRepeat(GLFW_KEY_UP, upHeld_, upNextRepeatAt_, [&] { selectPrev(); });
        handleRepeat(GLFW_KEY_DOWN, downHeld_, downNextRepeatAt_, [&] { selectNext(); });
        handleRepeat(
            GLFW_KEY_LEFT,
            leftHeld_,
            leftNextRepeatAt_,
            [&] {
                selectLeft(window, const_cast<input::ControlBindings&>(controls), controlsConfigPath);
            });
        handleRepeat(
            GLFW_KEY_RIGHT,
            rightHeld_,
            rightNextRepeatAt_,
            [&] {
                selectRight(window, const_cast<input::ControlBindings&>(controls), controlsConfigPath);
            });

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS &&
            (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        {
            resetAllSettings(window);
        }
    }
} // namespace ui

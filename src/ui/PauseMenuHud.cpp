#include "ui/PauseMenuController.h"
#include "ui/PauseMenuShared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {
    using namespace pause_menu_shared;

    OverlayRenderer::PauseMenuHud PauseMenuController::buildHud(
        const input::ControlBindings& controls,
        const bool advanceApplyIndicator) const
    {
        OverlayRenderer::PauseMenuHud hud{};
        hud.visible = open_;
        if (!open_) {
            return hud;
        }

        hud.awaitingBind = awaitingRebind_;
        hud.selectedRowIsControl = isControlPage();
        hud.firstVisibleLineIndex = std::max(0, scrollLineOffset_);
        hud.showApplyAction = hasPendingSettingsChanges();
        hud.hoverApplyAction = hoveredDisplayApplyAction_;
        hud.selectedApplyAction = focusTarget_ == FocusTarget::ApplyAction;
        hud.showResetWorldAction = settingsPage_ == SettingsPage::Simulation;
        hud.hoverResetWorldAction = hoveredResetWorldAction_;
        hud.selectedResetWorldAction = focusTarget_ == FocusTarget::ResetWorldAction;
        hud.showResetControlsAction = settingsPage_ == SettingsPage::Controls && hasControlChanges(controls);
        hud.hoverResetControlsAction = hoveredResetControlsAction_;
        hud.selectedResetControlsAction = focusTarget_ == FocusTarget::ResetControlsAction;
        hud.showResetIcon = true;
        hud.hoverResetIcon = hoveredResetIcon_;
        hud.selectedResetIcon = focusTarget_ == FocusTarget::ResetIcon;
        hud.selectedCloseAction = focusTarget_ == FocusTarget::CloseButton;
        hud.selectedExitAction = focusTarget_ == FocusTarget::ExitButton;
        hud.showResetConfirm = hasOpenPopup();
        hud.hoverResetConfirmYes = hoveredResetConfirmYes_;
        hud.hoverResetConfirmNo = hoveredResetConfirmNo_;
        hud.selectedResetConfirmYes = focusTarget_ == FocusTarget::PopupYes;
        hud.selectedResetConfirmNo = focusTarget_ == FocusTarget::PopupNo;
        switch (popupType_) {
            case PopupType::EnableDrawPath:
                hud.resetConfirmBodyText = "MAY IMPACT PERFORMANCE";
                break;
            case PopupType::ResetWorld:
                hud.resetConfirmBodyText = "RESET WORLD";
                break;
            case PopupType::ExitToHome:
                hud.resetConfirmBodyText = "EXIT TO HOME";
                break;
            case PopupType::ResetSettings:
                hud.resetConfirmBodyText = "RESET ALL SETTINGS";
                break;
            case PopupType::None:
                hud.resetConfirmBodyText.clear();
                break;
        }
        hud.footerHint =
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE VALUE   ENTER: APPLY OR REBIND   TAB: CHANGE PAGE   ESC: CLOSE";

        if (awaitingRebind_ && awaitingRebindAction_ >= 0 && awaitingRebindAction_ < controlCount()) {
            hud.pendingAction = input::rebindActions()[awaitingRebindAction_].label;
        }

        hud.statusLine = statusMessage_;
        hud.activePageIndex = static_cast<int>(settingsPage_);
        if (hud.statusLine.empty()) {
            const int rowForHint = hoveredSettingRow_ >= 0 ? hoveredSettingRow_ : selectedSettingRow_;
            if (!isControlPage() && rowForHint >= 0 && isSettingRowDisabled(rowForHint)) {
                hud.statusLine = disabledReasonForRow(rowForHint);
            }
        }

        int appliedRowOnCurrentPage = -1;
        if (applyIndicatorFrames_ > 0 && lastAppliedPage_ == settingsPage_) {
            appliedRowOnCurrentPage = lastAppliedRow_;
        }

        const DisplaySettings& displayRef =
            (settingsPage_ == SettingsPage::Display && pendingDisplayChanges_) ? draftDisplaySettings_ : appliedDisplaySettings_;
        const SimulationSettings& simRef =
            (settingsPage_ == SettingsPage::Simulation && pendingSelectionEdit_ && draftSettingsPage_ == settingsPage_)
                ? draftSimulationSettings_
                : appliedSimulationSettings_;
        const CameraSettings& cameraRef =
            (settingsPage_ == SettingsPage::Camera && pendingSelectionEdit_ && draftSettingsPage_ == settingsPage_)
                ? draftCameraSettings_
                : appliedCameraSettings_;
        const InterfaceSettings& interfaceRef =
            (settingsPage_ == SettingsPage::Interface && pendingSelectionEdit_ && draftSettingsPage_ == settingsPage_)
                ? draftInterfaceSettings_
                : appliedInterfaceSettings_;

        const auto normalized = [](const int idx, const int count) {
            if (count <= 1) {
                return 0.0f;
            }
            return std::clamp(static_cast<float>(idx) / static_cast<float>(count - 1), 0.0f, 1.0f);
        };
        const auto addHeader = [&](const std::string& title) {
            OverlayRenderer::PauseMenuHudLine line{};
            line.text = title;
            line.header = true;
            hud.lines.push_back(line);
        };
        const auto addRow = [&](const std::string& label,
                                const std::string& value,
                                const bool disabled,
                                const OverlayRenderer::PauseMenuControlType type,
                                const bool boolValue,
                                const bool showArrows,
                                const bool showSlider,
                                const float sliderT,
                                const std::string& hint = std::string()) {
            OverlayRenderer::PauseMenuHudLine line{};
            line.text = label;
            line.valueText = value;
            line.hintText = hint;
            line.header = false;
            line.disabled = disabled;
            line.controlType = type;
            line.boolValue = boolValue;
            line.showArrows = showArrows;
            line.showSlider = showSlider;
            line.sliderT = sliderT;
            hud.lines.push_back(line);
        };

        int firstSelectableLine = 1;
        int rowCount = settingCount();

        if (settingsPage_ == SettingsPage::Display) {
            addHeader("DISPLAY SETTINGS");
            addRow("WINDOW MODE",
                   windowModeLabel(displayRef.windowMode),
                   false,
                   OverlayRenderer::PauseMenuControlType::Choice,
                   false,
                   true,
                   false,
                   displayRef.windowMode == WindowMode::Windowed ? 1.0f : 0.0f,
                   "LEFT/RIGHT TO CYCLE");
            addRow("VSYNC",
                   displayRef.vsync ? "ON" : "OFF",
                   false,
                   OverlayRenderer::PauseMenuControlType::Toggle,
                   displayRef.vsync,
                   false,
                   false,
                   displayRef.vsync ? 1.0f : 0.0f,
                   "TOGGLE");

            if ((focusTarget_ == FocusTarget::SettingsRow ||
                 popupType_ == PopupType::ResetWorld ||
                 popupType_ == PopupType::EnableDrawPath) && selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, displaySettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, displaySettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Simulation) {
            addHeader("SIMULATION SETTINGS");
            const int minSpeedIdx = closestChoiceIndex(kMinSpeedChoices, simRef.minSimSpeed);
            const int maxSpeedIdx = closestChoiceIndex(kMaxSpeedChoices, simRef.maxSimSpeed);
            addRow("MIN SPEED", formatSpeedMultiplier(simRef.minSimSpeed), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(minSpeedIdx, static_cast<int>(kMinSpeedChoices.size())), "LEFT/RIGHT OR SLIDER");
            addRow("MAX SPEED", formatSpeedMultiplier(simRef.maxSimSpeed), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(maxSpeedIdx, static_cast<int>(kMaxSpeedChoices.size())), "LEFT/RIGHT OR SLIDER");
            const int gravityStrengthIdx = closestChoiceIndex(kGravityStrengthChoices, simRef.gravityStrength);
            addRow("GRAVITY", simRef.gravityEnabled ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, simRef.gravityEnabled, false, false, simRef.gravityEnabled ? 1.0f : 0.0f, "TOGGLE");
            addRow("GRAVITY STRENGTH", formatGravityMultiplier(simRef.gravityStrength), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(gravityStrengthIdx, static_cast<int>(kGravityStrengthChoices.size())), "LEFT/RIGHT OR SLIDER");
            addRow("COLLISIONS", simRef.collisionsEnabled ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, simRef.collisionsEnabled, false, false, simRef.collisionsEnabled ? 1.0f : 0.0f, "TOGGLE");
            const int collisionIterationsIdx = closestChoiceIndex(kCollisionIterationChoices, simRef.collisionIterations);
            addRow("COLLISION ITERATIONS", std::to_string(simRef.collisionIterations), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(collisionIterationsIdx, static_cast<int>(kCollisionIterationChoices.size())), "LEFT/RIGHT OR SLIDER");
            const int restitutionIdx = closestChoiceIndex(kGlobalRestitutionChoices, simRef.globalRestitution);
            addRow("GLOBAL BOUNCE", formatRestitutionPercent(simRef.globalRestitution), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(restitutionIdx, static_cast<int>(kGlobalRestitutionChoices.size())), "LEFT/RIGHT OR SLIDER");

            if ((focusTarget_ == FocusTarget::SettingsRow ||
                 popupType_ == PopupType::ResetWorld ||
                 popupType_ == PopupType::EnableDrawPath) && selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, simulationSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, simulationSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            addHeader("CAMERA SETTINGS");
            char sensitivityBuffer[32];
            std::snprintf(sensitivityBuffer, sizeof(sensitivityBuffer), "%.4f", cameraRef.lookSensitivity);
            const int sensitivityIdx = closestChoiceIndex(kLookSensitivityChoices, cameraRef.lookSensitivity);
            addRow("LOOK SENSITIVITY", sensitivityBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(sensitivityIdx, static_cast<int>(kLookSensitivityChoices.size())), "LEFT/RIGHT OR SLIDER");

            char moveSpeedBuffer[32];
            std::snprintf(moveSpeedBuffer, sizeof(moveSpeedBuffer), "%.0f", cameraRef.baseMoveSpeed);
            const int moveSpeedIdx = closestChoiceIndex(kBaseMoveSpeedChoices, cameraRef.baseMoveSpeed);
            addRow("BASE MOVE SPEED", moveSpeedBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(moveSpeedIdx, static_cast<int>(kBaseMoveSpeedChoices.size())), "LEFT/RIGHT OR SLIDER");

            addRow("INVERT Y", cameraRef.invertY ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, cameraRef.invertY, false, false, cameraRef.invertY ? 1.0f : 0.0f, "TOGGLE");

            char fovBuffer[32];
            std::snprintf(fovBuffer, sizeof(fovBuffer), "%.0f DEG", cameraRef.fovDegrees);
            const int fovIdx = closestChoiceIndex(kFovChoices, cameraRef.fovDegrees);
            addRow("FOV", fovBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(fovIdx, static_cast<int>(kFovChoices.size())), "LEFT/RIGHT OR SLIDER");

            if ((focusTarget_ == FocusTarget::SettingsRow ||
                 popupType_ == PopupType::ResetWorld ||
                 popupType_ == PopupType::EnableDrawPath) && selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, cameraSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, cameraSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            addHeader("INTERFACE");

            char scaleBuffer[32];
            std::snprintf(scaleBuffer, sizeof(scaleBuffer), "%.2fX", uiScaleValue(interfaceRef.uiScaleIndex));
            addRow("UI SCALE", scaleBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(interfaceRef.uiScaleIndex, static_cast<int>(kUiScaleChoices.size())), "LEFT/RIGHT OR SLIDER");
            addRow("SHOW HUD", interfaceRef.showHud ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.showHud, false, false, interfaceRef.showHud ? 1.0f : 0.0f, "TOGGLE");
            addRow("CROSSHAIR", interfaceRef.showCrosshair ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.showCrosshair, false, false, interfaceRef.showCrosshair ? 1.0f : 0.0f, "TOGGLE");
            addRow("DEBUG STATS", interfaceRef.showPhysicsStats ? "ON" : "OFF", !interfaceRef.showHud, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.showPhysicsStats, false, false, interfaceRef.showPhysicsStats ? 1.0f : 0.0f, !interfaceRef.showHud ? "ENABLE SHOW HUD FIRST" : "TOGGLE");
            addRow("DRAW PATH", interfaceRef.drawPath ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.drawPath, false, false, interfaceRef.drawPath ? 1.0f : 0.0f, "TOGGLE - MAY IMPACT PERFORMANCE");
            addRow("OBJECT INFO", interfaceRef.objectInfo ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfo, false, false, interfaceRef.objectInfo ? 1.0f : 0.0f, "TOGGLE");
            addRow("OBJECT INFO MATERIAL", interfaceRef.objectInfoMaterial ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoMaterial, false, false, interfaceRef.objectInfoMaterial ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO VELOCITY", interfaceRef.objectInfoVelocity ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoVelocity, false, false, interfaceRef.objectInfoVelocity ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO MASS", interfaceRef.objectInfoMass ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoMass, false, false, interfaceRef.objectInfoMass ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO RADIUS", interfaceRef.objectInfoRadius ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoRadius, false, false, interfaceRef.objectInfoRadius ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO ANGULAR SPEED", interfaceRef.objectInfoAngularSpeed ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoAngularSpeed, false, false, interfaceRef.objectInfoAngularSpeed ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO BODY TYPE", interfaceRef.objectInfoBodyType ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoBodyType, false, false, interfaceRef.objectInfoBodyType ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            if ((focusTarget_ == FocusTarget::SettingsRow ||
                 popupType_ == PopupType::ResetWorld ||
                 popupType_ == PopupType::EnableDrawPath) && selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, interfaceSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, interfaceSettingRowCount() - 1);
            }
        } else {
            addHeader("CONTROLS");
            firstSelectableLine = static_cast<int>(hud.lines.size());
            rowCount = controlCount();
            const int actionCount = controlCount();
            for (int i = 0; i < actionCount; ++i) {
                const auto& action = input::rebindActions()[i];
                const int keyCode = controls.*(action.member);
                addRow(action.label, input::keyNameForCode(keyCode), false, OverlayRenderer::PauseMenuControlType::Rebind, false, false, false, 0.0f, "ENTER/SPACE OR CLICK TO REBIND");
            }

            if ((focusTarget_ == FocusTarget::SettingsRow ||
                 popupType_ == PopupType::ResetWorld ||
                 popupType_ == PopupType::EnableDrawPath) && selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = firstSelectableLine + std::clamp(selectedSettingRow_, 0, actionCount - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = firstSelectableLine + std::clamp(appliedRowOnCurrentPage, 0, actionCount - 1);
            }
        }

        if (rowCount > 0 && hoveredSettingRow_ >= 0) {
            hud.hoveredSettingLineIndex = firstSelectableLine + std::clamp(hoveredSettingRow_, 0, rowCount - 1);
        }

        if (advanceApplyIndicator && applyIndicatorFrames_ > 0) {
            --applyIndicatorFrames_;
            if (applyIndicatorFrames_ == 0) {
                lastAppliedRow_ = -1;
            }
        }

        return hud;
    }
} // namespace ui

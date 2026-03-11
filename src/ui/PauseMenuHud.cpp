#include "ui/PauseMenuController.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace ui {
    namespace {
        constexpr std::array<float, 7> kUiScaleChoices{{
            0.80f, 0.90f, 1.00f, 1.10f, 1.20f, 1.30f, 1.40f,
        }};

        constexpr std::array<double, 9> kMinSpeedChoices{{
            1.0 / 256.0, 1.0 / 128.0, 1.0 / 64.0, 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0,
        }};

        constexpr std::array<double, 9> kMaxSpeedChoices{{
            1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0,
        }};

        constexpr double kDefaultGravityStrength = 6.6743e-11;
        constexpr std::array<double, 12> kGravityStrengthChoices{{
            kDefaultGravityStrength * 0.25, kDefaultGravityStrength * 0.50, kDefaultGravityStrength * 0.75,
            kDefaultGravityStrength * 1.00, kDefaultGravityStrength * 1.25, kDefaultGravityStrength * 1.50,
            kDefaultGravityStrength * 1.75, kDefaultGravityStrength * 2.00, kDefaultGravityStrength * 2.25,
            kDefaultGravityStrength * 2.50, kDefaultGravityStrength * 2.75, kDefaultGravityStrength * 3.00,
        }};

        constexpr std::array<int, 8> kCollisionIterationChoices{{1, 2, 3, 4, 5, 6, 8, 10}};

        constexpr std::array<float, 9> kLookSensitivityChoices{{
            0.0008f, 0.0012f, 0.0016f, 0.0020f, 0.0025f, 0.0030f, 0.0038f, 0.0048f, 0.0060f,
        }};

        constexpr std::array<float, 8> kBaseMoveSpeedChoices{{
            10.0f, 20.0f, 30.0f, 40.0f, 60.0f, 80.0f, 100.0f, 140.0f,
        }};

        constexpr std::array<float, 8> kFovChoices{{
            50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f,
        }};

        template <typename T, std::size_t N>
        int closestChoiceIndex(const std::array<T, N>& choices, const T value) {
            int bestIdx = 0;
            double bestDiff = std::fabs(static_cast<double>(choices[0] - value));
            for (std::size_t i = 1; i < N; ++i) {
                const double diff = std::fabs(static_cast<double>(choices[i] - value));
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestIdx = static_cast<int>(i);
                }
            }
            return bestIdx;
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
            if (const double multiplier = value / kDefaultGravityStrength; multiplier >= 10.0) {
                std::snprintf(buffer, sizeof(buffer), "%.0fX", multiplier);
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.2fX", multiplier);
            }
            return buffer;
        }
    } // namespace

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
        hud.showDisplayApplyAction = settingsPage_ == SettingsPage::Display && pendingDisplayChanges_;
        hud.hoverDisplayApplyAction = hoveredDisplayApplyAction_;
        hud.showResetControlsAction = settingsPage_ == SettingsPage::Controls && hasControlChanges(controls);
        hud.hoverResetControlsAction = hoveredResetControlsAction_;
        hud.showResetIcon = true;
        hud.hoverResetIcon = hoveredResetIcon_;
        hud.showResetConfirm = confirmingResetSettings_;
        hud.hoverResetConfirmYes = hoveredResetConfirmYes_;
        hud.hoverResetConfirmNo = hoveredResetConfirmNo_;
        hud.footerHint =
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   ENTER: APPLY OR REBIND   TAB: CHANGE PAGE   ESC: CLOSE";

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
                   "CYCLE WITH LEFT/RIGHT");
            addRow("VSYNC",
                   displayRef.vsync ? "ON" : "OFF",
                   false,
                   OverlayRenderer::PauseMenuControlType::Toggle,
                   displayRef.vsync,
                   false,
                   false,
                   displayRef.vsync ? 1.0f : 0.0f,
                   "TOGGLE");

            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, displaySettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, displaySettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Simulation) {
            addHeader("SIMULATION SETTINGS");
            const int minSpeedIdx = closestChoiceIndex(kMinSpeedChoices, simRef.minSimSpeed);
            const int maxSpeedIdx = closestChoiceIndex(kMaxSpeedChoices, simRef.maxSimSpeed);
            addRow("MIN SPEED", formatSpeedMultiplier(simRef.minSimSpeed), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(minSpeedIdx, static_cast<int>(kMinSpeedChoices.size())), "ARROWS / WHEEL / SLIDER");
            addRow("MAX SPEED", formatSpeedMultiplier(simRef.maxSimSpeed), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(maxSpeedIdx, static_cast<int>(kMaxSpeedChoices.size())), "ARROWS / WHEEL / SLIDER");
            const int gravityStrengthIdx = closestChoiceIndex(kGravityStrengthChoices, simRef.gravityStrength);
            addRow("GRAVITY", simRef.gravityEnabled ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, simRef.gravityEnabled, false, false, simRef.gravityEnabled ? 1.0f : 0.0f, "TOGGLE");
            addRow("GRAVITY STRENGTH", formatGravityMultiplier(simRef.gravityStrength), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(gravityStrengthIdx, static_cast<int>(kGravityStrengthChoices.size())), "ARROWS / WHEEL / SLIDER");
            const int collisionIterationsIdx = closestChoiceIndex(kCollisionIterationChoices, simRef.collisionIterations);
            addRow("COLLISION ITERATIONS", std::to_string(simRef.collisionIterations), false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(collisionIterationsIdx, static_cast<int>(kCollisionIterationChoices.size())), "ARROWS / WHEEL / SLIDER");

            if (selectedSettingRow_ >= 0) {
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
            addRow("LOOK SENSITIVITY", sensitivityBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(sensitivityIdx, static_cast<int>(kLookSensitivityChoices.size())), "ARROWS / WHEEL / SLIDER");

            char moveSpeedBuffer[32];
            std::snprintf(moveSpeedBuffer, sizeof(moveSpeedBuffer), "%.0f", cameraRef.baseMoveSpeed);
            const int moveSpeedIdx = closestChoiceIndex(kBaseMoveSpeedChoices, cameraRef.baseMoveSpeed);
            addRow("BASE MOVE SPEED", moveSpeedBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(moveSpeedIdx, static_cast<int>(kBaseMoveSpeedChoices.size())), "ARROWS / WHEEL / SLIDER");

            addRow("INVERT Y", cameraRef.invertY ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, cameraRef.invertY, false, false, cameraRef.invertY ? 1.0f : 0.0f, "TOGGLE");

            char fovBuffer[32];
            std::snprintf(fovBuffer, sizeof(fovBuffer), "%.0f DEG", cameraRef.fovDegrees);
            const int fovIdx = closestChoiceIndex(kFovChoices, cameraRef.fovDegrees);
            addRow("FOV", fovBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(fovIdx, static_cast<int>(kFovChoices.size())), "ARROWS / WHEEL / SLIDER");

            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, cameraSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex = 1 + std::clamp(appliedRowOnCurrentPage, 0, cameraSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            addHeader("INTERFACE");

            char scaleBuffer[32];
            std::snprintf(scaleBuffer, sizeof(scaleBuffer), "%.2fX", uiScaleValue(interfaceRef.uiScaleIndex));
            addRow("UI SCALE", scaleBuffer, false, OverlayRenderer::PauseMenuControlType::Numeric, false, true, true, normalized(interfaceRef.uiScaleIndex, static_cast<int>(kUiScaleChoices.size())), "ARROWS / WHEEL / SLIDER");
            addRow("SHOW HUD", interfaceRef.showHud ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.showHud, false, false, interfaceRef.showHud ? 1.0f : 0.0f, "TOGGLE");
            addRow("CROSSHAIR", interfaceRef.showCrosshair ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.showCrosshair, false, false, interfaceRef.showCrosshair ? 1.0f : 0.0f, "TOGGLE");
            addRow("OBJECT INFO", interfaceRef.objectInfo ? "ON" : "OFF", false, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfo, false, false, interfaceRef.objectInfo ? 1.0f : 0.0f, "TOGGLE");
            addRow("OBJECT INFO MATERIAL", interfaceRef.objectInfoMaterial ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoMaterial, false, false, interfaceRef.objectInfoMaterial ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO VELOCITY", interfaceRef.objectInfoVelocity ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoVelocity, false, false, interfaceRef.objectInfoVelocity ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO MASS", interfaceRef.objectInfoMass ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoMass, false, false, interfaceRef.objectInfoMass ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow("OBJECT INFO RADIUS", interfaceRef.objectInfoRadius ? "ON" : "OFF", !interfaceRef.objectInfo, OverlayRenderer::PauseMenuControlType::Toggle, interfaceRef.objectInfoRadius, false, false, interfaceRef.objectInfoRadius ? 1.0f : 0.0f, !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            if (selectedSettingRow_ >= 0) {
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

            if (selectedSettingRow_ >= 0) {
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

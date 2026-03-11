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

        int wrapIndex(const int idx, const int count) {
            if (count <= 0) {
                return 0;
            }
            const int mod = idx % count;
            return mod < 0 ? (mod + count) : mod;
        }

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
    } // namespace

    float PauseMenuController::uiScale() const {
        return uiScaleValue(appliedInterfaceSettings_.uiScaleIndex);
    }

    int PauseMenuController::settingCount() const {
        switch (settingsPage_) {
            case SettingsPage::Display:
                return displaySettingRowCount();
            case SettingsPage::Simulation:
                return simulationSettingRowCount();
            case SettingsPage::Camera:
                return cameraSettingRowCount();
            case SettingsPage::Interface:
                return interfaceSettingRowCount();
            case SettingsPage::Controls:
                return controlCount();
        }
        return 0;
    }

    int PauseMenuController::controlCount() {
        return static_cast<int>(input::rebindActions().size());
    }

    int PauseMenuController::displaySettingRowCount() {
        return 2;
    }

    int PauseMenuController::simulationSettingRowCount() {
        return 5;
    }

    int PauseMenuController::cameraSettingRowCount() {
        return 4;
    }

    int PauseMenuController::interfaceSettingRowCount() {
        return 8;
    }

    float PauseMenuController::uiScaleValue(int idx) {
        idx = std::clamp(idx, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
        return kUiScaleChoices[static_cast<std::size_t>(idx)];
    }

    const char* PauseMenuController::windowModeLabel(const WindowMode mode) {
        switch (mode) {
            case WindowMode::Borderless:
                return "BORDERLESS";
            case WindowMode::Windowed:
                return "WINDOWED";
        }
        return "UNKNOWN";
    }

    int PauseMenuController::wrapSelectionRow(const int row, const int count) {
        return wrapIndex(row, count);
    }

    const char* PauseMenuController::settingsPageLabel(const SettingsPage page) {
        switch (page) {
            case SettingsPage::Display:
                return "DISPLAY";
            case SettingsPage::Simulation:
                return "SIMULATION";
            case SettingsPage::Camera:
                return "CAMERA";
            case SettingsPage::Interface:
                return "INTERFACE";
            case SettingsPage::Controls:
                return "CONTROLS";
        }
        return "UNKNOWN";
    }

    std::string PauseMenuController::formatMenuValue(const char* value) {
        return value;
    }

    std::string PauseMenuController::formatMenuValue(const std::string& value) {
        return value;
    }

    bool PauseMenuController::isControlPage() const {
        return settingsPage_ == SettingsPage::Controls;
    }

    bool PauseMenuController::hasControlChanges(const input::ControlBindings& controls) {
        constexpr input::ControlBindings defaults{};
        return controls.freeze != defaults.freeze ||
               controls.speedDown != defaults.speedDown ||
               controls.speedUp != defaults.speedUp ||
               controls.speedReset != defaults.speedReset ||
               controls.moveForward != defaults.moveForward ||
               controls.moveBack != defaults.moveBack ||
               controls.moveLeft != defaults.moveLeft ||
               controls.moveRight != defaults.moveRight ||
               controls.moveUp != defaults.moveUp ||
               controls.moveDown != defaults.moveDown ||
               controls.cameraBoost != defaults.cameraBoost;
    }

    void PauseMenuController::setSelectedSettingRow(const int row) {
        selectedSettingRow_ = row;
    }

    void PauseMenuController::discardPendingEdit() {
        draftDisplaySettings_ = appliedDisplaySettings_;
        draftSimulationSettings_ = appliedSimulationSettings_;
        draftCameraSettings_ = appliedCameraSettings_;
        draftInterfaceSettings_ = appliedInterfaceSettings_;
        pendingDisplayChanges_ = false;
        draftSettingsPage_ = settingsPage_;
        draftSelectionRow_ = -1;
        pendingSelectionEdit_ = false;
    }

    void PauseMenuController::ensureDraftForSelectedRow() {
        if (pendingSelectionEdit_ &&
            (draftSettingsPage_ != settingsPage_ || draftSelectionRow_ != selectedSettingRow_))
        {
            discardPendingEdit();
        }

        if (!pendingSelectionEdit_) {
            draftDisplaySettings_ = appliedDisplaySettings_;
            draftSimulationSettings_ = appliedSimulationSettings_;
            draftCameraSettings_ = appliedCameraSettings_;
            draftInterfaceSettings_ = appliedInterfaceSettings_;
            draftSettingsPage_ = settingsPage_;
            draftSelectionRow_ = selectedSettingRow_;
            pendingSelectionEdit_ = true;
        }
    }

    void PauseMenuController::switchPage(const int delta) {
        switchToPage(static_cast<SettingsPage>(wrapIndex(static_cast<int>(settingsPage_) + delta, 5)));
    }

    void PauseMenuController::switchToPage(const SettingsPage page) {
        settingsPage_ = page;
        discardPendingEdit();
        awaitingRebind_ = false;
        awaitingRebindAction_ = -1;
        ignoreNextRebindMousePress_ = false;
        hoveredSettingRow_ = -1;
        hoveredDisplayApplyAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;

        setSelectedSettingRow(-1);
        statusMessage_.clear();
    }

    void PauseMenuController::commitSelectedSetting(GLFWwindow* window) {
        if (isControlPage()) {
            return;
        }

        if (settingsPage_ == SettingsPage::Display) {
            if (!pendingDisplayChanges_) {
                statusMessage_.clear();
                return;
            }
            appliedDisplaySettings_ = draftDisplaySettings_;
            applyDisplaySettings(window, appliedDisplaySettings_);
        } else if (settingsPage_ == SettingsPage::Simulation) {
            if (!pendingSelectionEdit_ || draftSettingsPage_ != settingsPage_ || draftSelectionRow_ != selectedSettingRow_) {
                statusMessage_.clear();
                return;
            }
            appliedSimulationSettings_ = draftSimulationSettings_;
        } else if (settingsPage_ == SettingsPage::Camera) {
            if (!pendingSelectionEdit_ || draftSettingsPage_ != settingsPage_ || draftSelectionRow_ != selectedSettingRow_) {
                statusMessage_.clear();
                return;
            }
            appliedCameraSettings_ = draftCameraSettings_;
        } else if (settingsPage_ == SettingsPage::Interface) {
            if (!pendingSelectionEdit_ || draftSettingsPage_ != settingsPage_ || draftSelectionRow_ != selectedSettingRow_) {
                statusMessage_.clear();
                return;
            }
            appliedInterfaceSettings_ = draftInterfaceSettings_;
        }
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();

        pendingDisplayChanges_ = false;
        pendingSelectionEdit_ = false;
        draftSelectionRow_ = -1;
        draftSettingsPage_ = settingsPage_;
    }

    bool PauseMenuController::beginRebindForRow(const int row) {
        if (!isControlPage()) {
            return false;
        }
        const int actionCount = controlCount();
        if (actionCount <= 0) {
            statusMessage_ = "NO CONTROLS TO REBIND";
            return false;
        }
        if (row < 0 || row >= actionCount) {
            return false;
        }
        awaitingRebindAction_ = row;
        awaitingRebind_ = true;
        ignoreNextRebindMousePress_ = false;
        statusMessage_ = "PRESS A KEY OR MOUSE BUTTON...  ESC/BACK/CANCEL TO ABORT";
        return true;
    }

    bool PauseMenuController::applyRebindCode(
        const int code,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        if (awaitingRebindAction_ < 0 || awaitingRebindAction_ >= controlCount()) {
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            ignoreNextRebindMousePress_ = false;
            statusMessage_ = "REBIND ERROR";
            return false;
        }

        if (!input::isBindableKey(code)) {
            statusMessage_ = "INPUT NOT BINDABLE";
            return false;
        }

        const auto& actions = input::rebindActions();
        for (int actionIndex = 0; actionIndex < controlCount(); ++actionIndex) {
            if (actionIndex == awaitingRebindAction_) {
                continue;
            }
            if (controls.*(actions[actionIndex].member) == code) {
                statusMessage_ = "INPUT ALREADY IN USE";
                return false;
            }
        }

        controls.*(actions[awaitingRebindAction_].member) = code;
        input::saveControlBindings(controls, controlsConfigPath);

        statusMessage_.clear();
        markAppliedSelection();
        awaitingRebind_ = false;
        awaitingRebindAction_ = -1;
        ignoreNextRebindMousePress_ = false;
        return true;
    }

    void PauseMenuController::selectNext() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
            discardPendingEdit();
        }
        setSelectedSettingRow(wrapSelectionRow(selectedSettingRow_ + 1, count));
        statusMessage_.clear();
    }

    void PauseMenuController::selectPrev() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
            discardPendingEdit();
        }
        setSelectedSettingRow(wrapSelectionRow(selectedSettingRow_ - 1, count));
        statusMessage_.clear();
    }

    bool PauseMenuController::isSettingRowDisabled(const int row) const {
        if (row < 0) {
            return true;
        }
        if (settingsPage_ == SettingsPage::Display) {
            return false;
        }
        if (settingsPage_ == SettingsPage::Interface) {
            return row >= 4 && row <= 7 && !appliedInterfaceSettings_.objectInfo;
        }
        return false;
    }

    std::string PauseMenuController::disabledReasonForRow(const int row) const {
        if (settingsPage_ == SettingsPage::Interface && row >= 4 && row <= 7) {
            return "ENABLE OBJECT INFO FIRST";
        }
        return "SETTING LOCKED";
    }

    OverlayRenderer::PauseMenuControlType PauseMenuController::controlTypeForRow(const int row) const {
        if (row < 0) {
            return OverlayRenderer::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Display) {
            if (row == 0) {
                return OverlayRenderer::PauseMenuControlType::Choice;
            }
            return row == 1 ? OverlayRenderer::PauseMenuControlType::Toggle : OverlayRenderer::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            if (row == 2) {
                return OverlayRenderer::PauseMenuControlType::Toggle;
            }
            return row <= 4 ? OverlayRenderer::PauseMenuControlType::Numeric : OverlayRenderer::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Camera) {
            if (row == 2) {
                return OverlayRenderer::PauseMenuControlType::Toggle;
            }
            return row <= 3 ? OverlayRenderer::PauseMenuControlType::Numeric : OverlayRenderer::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Interface) {
            if (row == 0) {
                return OverlayRenderer::PauseMenuControlType::Numeric;
            }
            if (row <= 7) {
                return OverlayRenderer::PauseMenuControlType::Toggle;
            }
            return OverlayRenderer::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Controls) {
            return row < controlCount() ? OverlayRenderer::PauseMenuControlType::Rebind : OverlayRenderer::PauseMenuControlType::Action;
        }
        return OverlayRenderer::PauseMenuControlType::None;
    }

    bool PauseMenuController::supportsContinuousAdjust(const int row) const {
        const auto type = controlTypeForRow(row);
        return type == OverlayRenderer::PauseMenuControlType::Numeric ||
               type == OverlayRenderer::PauseMenuControlType::Choice;
    }

    int PauseMenuController::optionCountForRow(const int row) const {
        if (row < 0) {
            return 0;
        }
        if (settingsPage_ == SettingsPage::Display) {
            switch (row) {
                case 0: return 2;
                case 1: return 2;
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            switch (row) {
                case 0: return static_cast<int>(kMinSpeedChoices.size());
                case 1: return static_cast<int>(kMaxSpeedChoices.size());
                case 2: return 2;
                case 3: return static_cast<int>(kGravityStrengthChoices.size());
                case 4: return static_cast<int>(kCollisionIterationChoices.size());
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Camera) {
            switch (row) {
                case 0: return static_cast<int>(kLookSensitivityChoices.size());
                case 1: return static_cast<int>(kBaseMoveSpeedChoices.size());
                case 2: return 2;
                case 3: return static_cast<int>(kFovChoices.size());
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Interface) {
            if (row == 0) {
                return static_cast<int>(kUiScaleChoices.size());
            }
            if (row >= 1 && row <= 7) {
                return 2;
            }
        }
        return 0;
    }

    int PauseMenuController::optionIndexForRow(const int row) const {
        if (row < 0) {
            return 0;
        }
        if (settingsPage_ == SettingsPage::Display) {
            const DisplaySettings& displayRef =
                (pendingDisplayChanges_ && settingsPage_ == SettingsPage::Display) ? draftDisplaySettings_ : appliedDisplaySettings_;
            switch (row) {
                case 0: return static_cast<int>(displayRef.windowMode);
                case 1: return displayRef.vsync ? 1 : 0;
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            switch (row) {
                case 0: return closestChoiceIndex(kMinSpeedChoices, appliedSimulationSettings_.minSimSpeed);
                case 1: return closestChoiceIndex(kMaxSpeedChoices, appliedSimulationSettings_.maxSimSpeed);
                case 2: return appliedSimulationSettings_.gravityEnabled ? 1 : 0;
                case 3: return closestChoiceIndex(kGravityStrengthChoices, appliedSimulationSettings_.gravityStrength);
                case 4: return closestChoiceIndex(kCollisionIterationChoices, appliedSimulationSettings_.collisionIterations);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Camera) {
            switch (row) {
                case 0: return closestChoiceIndex(kLookSensitivityChoices, appliedCameraSettings_.lookSensitivity);
                case 1: return closestChoiceIndex(kBaseMoveSpeedChoices, appliedCameraSettings_.baseMoveSpeed);
                case 2: return appliedCameraSettings_.invertY ? 1 : 0;
                case 3: return closestChoiceIndex(kFovChoices, appliedCameraSettings_.fovDegrees);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Interface) {
            switch (row) {
                case 0:
                    return std::clamp(appliedInterfaceSettings_.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
                case 1: return appliedInterfaceSettings_.showHud ? 1 : 0;
                case 2: return appliedInterfaceSettings_.showCrosshair ? 1 : 0;
                case 3: return appliedInterfaceSettings_.objectInfo ? 1 : 0;
                case 4: return appliedInterfaceSettings_.objectInfoMaterial ? 1 : 0;
                case 5: return appliedInterfaceSettings_.objectInfoVelocity ? 1 : 0;
                case 6: return appliedInterfaceSettings_.objectInfoMass ? 1 : 0;
                case 7: return appliedInterfaceSettings_.objectInfoRadius ? 1 : 0;
                default: return 0;
            }
        }
        return 0;
    }

    bool PauseMenuController::setOptionIndexForRow(const int row, const int targetIndex) {
        if (isControlPage()) {
            return false;
        }
        const int optionCount = optionCountForRow(row);
        if (optionCount <= 0) {
            return false;
        }
        if (isSettingRowDisabled(row)) {
            statusMessage_ = disabledReasonForRow(row);
            return false;
        }

        setSelectedSettingRow(std::clamp(row, 0, settingCount() - 1));
        int current = optionIndexForRow(selectedSettingRow_);
        const int target = std::clamp(targetIndex, 0, optionCount - 1);
        while (current != target) {
            const int previous = current;
            adjustSelectedSetting(target > current ? 1 : -1);
            current = optionIndexForRow(selectedSettingRow_);
            if (current == previous) {
                break;
            }
        }
        return current == target;
    }

    void PauseMenuController::adjustSelectedSetting(const int delta) {
        if (isControlPage()) {
            return;
        }

        if (settingsPage_ == SettingsPage::Display) {
            if (!pendingDisplayChanges_) {
                draftDisplaySettings_ = appliedDisplaySettings_;
            }
            pendingDisplayChanges_ = true;

            auto& settings = draftDisplaySettings_;
            switch (selectedSettingRow_) {
                case 0:
                    settings.windowMode = settings.windowMode == WindowMode::Borderless ? WindowMode::Windowed : WindowMode::Borderless;
                    break;
                case 1:
                    settings.vsync = !settings.vsync;
                    break;
                default:
                    break;
            }
            pendingDisplayChanges_ = hasPendingDisplayChanges();
            if (!pendingDisplayChanges_) {
                draftDisplaySettings_ = appliedDisplaySettings_;
            }
            statusMessage_.clear();
            return;
        }

        ensureDraftForSelectedRow();

        if (settingsPage_ == SettingsPage::Simulation) {
            auto& settings = draftSimulationSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = std::clamp(closestChoiceIndex(kMinSpeedChoices, settings.minSimSpeed) + delta, 0, static_cast<int>(kMinSpeedChoices.size()) - 1);
                    settings.minSimSpeed = kMinSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.maxSimSpeed < settings.minSimSpeed) {
                        settings.maxSimSpeed = settings.minSimSpeed;
                    }
                    break;
                }
                case 1: {
                    const int idx = std::clamp(closestChoiceIndex(kMaxSpeedChoices, settings.maxSimSpeed) + delta, 0, static_cast<int>(kMaxSpeedChoices.size()) - 1);
                    settings.maxSimSpeed = kMaxSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.minSimSpeed > settings.maxSimSpeed) {
                        settings.minSimSpeed = settings.maxSimSpeed;
                    }
                    break;
                }
                case 2:
                    settings.gravityEnabled = !settings.gravityEnabled;
                    break;
                case 3: {
                    const int idx = std::clamp(closestChoiceIndex(kGravityStrengthChoices, settings.gravityStrength) + delta, 0, static_cast<int>(kGravityStrengthChoices.size()) - 1);
                    settings.gravityStrength = kGravityStrengthChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 4: {
                    const int idx = std::clamp(closestChoiceIndex(kCollisionIterationChoices, settings.collisionIterations) + delta, 0, static_cast<int>(kCollisionIterationChoices.size()) - 1);
                    settings.collisionIterations = kCollisionIterationChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            auto& settings = draftCameraSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = std::clamp(closestChoiceIndex(kLookSensitivityChoices, settings.lookSensitivity) + delta, 0, static_cast<int>(kLookSensitivityChoices.size()) - 1);
                    settings.lookSensitivity = kLookSensitivityChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 1: {
                    const int idx = std::clamp(closestChoiceIndex(kBaseMoveSpeedChoices, settings.baseMoveSpeed) + delta, 0, static_cast<int>(kBaseMoveSpeedChoices.size()) - 1);
                    settings.baseMoveSpeed = kBaseMoveSpeedChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 2:
                    settings.invertY = !settings.invertY;
                    break;
                case 3: {
                    const int idx = std::clamp(closestChoiceIndex(kFovChoices, settings.fovDegrees) + delta, 0, static_cast<int>(kFovChoices.size()) - 1);
                    settings.fovDegrees = kFovChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            auto& settings = draftInterfaceSettings_;
            switch (selectedSettingRow_) {
                case 0:
                    settings.uiScaleIndex = std::clamp(settings.uiScaleIndex + delta, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
                    break;
                case 1:
                    settings.showHud = !settings.showHud;
                    break;
                case 2:
                    settings.showCrosshair = !settings.showCrosshair;
                    break;
                case 3:
                    settings.objectInfo = !settings.objectInfo;
                    break;
                case 4:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMaterial = !settings.objectInfoMaterial;
                    break;
                case 5:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoVelocity = !settings.objectInfoVelocity;
                    break;
                case 6:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMass = !settings.objectInfoMass;
                    break;
                case 7:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoRadius = !settings.objectInfoRadius;
                    break;
                default:
                    break;
            }
        }

        if (settingsPage_ == SettingsPage::Simulation) {
            appliedSimulationSettings_ = draftSimulationSettings_;
        } else if (settingsPage_ == SettingsPage::Camera) {
            appliedCameraSettings_ = draftCameraSettings_;
        } else if (settingsPage_ == SettingsPage::Interface) {
            appliedInterfaceSettings_ = draftInterfaceSettings_;
        }

        markAppliedSelection();
        pendingSelectionEdit_ = false;
        draftSelectionRow_ = -1;
        draftSettingsPage_ = settingsPage_;
        statusMessage_.clear();
        saveSettings();
    }

    void PauseMenuController::resetAllSettings(GLFWwindow* window) {
        appliedDisplaySettings_ = DisplaySettings{};
        appliedSimulationSettings_ = SimulationSettings{};
        appliedCameraSettings_ = CameraSettings{};
        appliedInterfaceSettings_ = InterfaceSettings{};
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedDisplaySettings_);
        discardPendingEdit();
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();
    }

    bool PauseMenuController::hasPendingDisplayChanges() const {
        return draftDisplaySettings_.windowMode != appliedDisplaySettings_.windowMode ||
               draftDisplaySettings_.vsync != appliedDisplaySettings_.vsync;
    }

    void PauseMenuController::normalizeAppliedSettings() {
        appliedSimulationSettings_.minSimSpeed =
            kMinSpeedChoices[static_cast<std::size_t>(closestChoiceIndex(kMinSpeedChoices, appliedSimulationSettings_.minSimSpeed))];
        appliedSimulationSettings_.maxSimSpeed =
            kMaxSpeedChoices[static_cast<std::size_t>(closestChoiceIndex(kMaxSpeedChoices, appliedSimulationSettings_.maxSimSpeed))];
        appliedSimulationSettings_.gravityStrength =
            kGravityStrengthChoices[static_cast<std::size_t>(closestChoiceIndex(kGravityStrengthChoices, appliedSimulationSettings_.gravityStrength))];
        appliedSimulationSettings_.collisionIterations =
            kCollisionIterationChoices[static_cast<std::size_t>(closestChoiceIndex(kCollisionIterationChoices, appliedSimulationSettings_.collisionIterations))];
        if (appliedSimulationSettings_.maxSimSpeed < appliedSimulationSettings_.minSimSpeed) {
            appliedSimulationSettings_.maxSimSpeed = appliedSimulationSettings_.minSimSpeed;
        }

        appliedCameraSettings_.lookSensitivity =
            kLookSensitivityChoices[static_cast<std::size_t>(closestChoiceIndex(kLookSensitivityChoices, appliedCameraSettings_.lookSensitivity))];
        appliedCameraSettings_.baseMoveSpeed =
            kBaseMoveSpeedChoices[static_cast<std::size_t>(closestChoiceIndex(kBaseMoveSpeedChoices, appliedCameraSettings_.baseMoveSpeed))];
        appliedCameraSettings_.fovDegrees =
            kFovChoices[static_cast<std::size_t>(closestChoiceIndex(kFovChoices, appliedCameraSettings_.fovDegrees))];

        appliedInterfaceSettings_.uiScaleIndex =
            std::clamp(appliedInterfaceSettings_.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
    }

    void PauseMenuController::markAppliedSelection() const {
        lastAppliedPage_ = settingsPage_;
        lastAppliedRow_ = selectedSettingRow_;
        applyIndicatorFrames_ = 120;
    }
} // namespace ui

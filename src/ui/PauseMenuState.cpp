#include "ui/PauseMenuController.h"
#include "ui/PauseMenuShared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {
    namespace {
        int wrapIndex(const int idx, const int count) {
            if (count <= 0) {
                return 0;
            }
            const int mod = idx % count;
            return mod < 0 ? (mod + count) : mod;
        }
    } // namespace

    using namespace pause_menu_shared;

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
        return 7;
    }

    int PauseMenuController::cameraSettingRowCount() {
        return 4;
    }

    int PauseMenuController::interfaceSettingRowCount() {
        return 12;
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
        if (row >= 0) {
            focusTarget_ = FocusTarget::SettingsRow;
        }
    }

    void PauseMenuController::setFocusTarget(const FocusTarget target) {
        focusTarget_ = target;
    }

    bool PauseMenuController::hasOpenPopup() const {
        return popupType_ != PopupType::None;
    }

    bool PauseMenuController::popupUsesSingleAcknowledge() const {
        return popupType_ == PopupType::EnableDrawPath;
    }

    void PauseMenuController::openPopup(
        const PopupType type,
        const FocusTarget returnTarget,
        const bool selectDefaultAction)
    {
        popupType_ = type;
        popupReturnTarget_ = returnTarget;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;
        focusTarget_ = selectDefaultAction ? FocusTarget::PopupYes : returnTarget;
    }

    void PauseMenuController::closePopup(const bool restoreFocus) {
        popupType_ = PopupType::None;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;
        focusTarget_ = restoreFocus ? popupReturnTarget_ : FocusTarget::SettingsRow;
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
        hoveredResetWorldAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;
        popupType_ = PopupType::None;
        popupReturnTarget_ = FocusTarget::SettingsRow;

        setSelectedSettingRow(-1);
        scrollLineOffset_ = 0;
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
        if (hasOpenPopup()) {
            if (focusTarget_ != FocusTarget::PopupYes && focusTarget_ != FocusTarget::PopupNo) {
                focusTarget_ = FocusTarget::PopupYes;
            }
            return;
        }
        if (count <= 0 && focusTarget_ == FocusTarget::SettingsRow) {
            return;
        }

        if (focusTarget_ == FocusTarget::ResetIcon ||
            focusTarget_ == FocusTarget::CloseButton ||
            focusTarget_ == FocusTarget::ExitButton)
        {
            if (focusTarget_ == FocusTarget::ResetIcon) {
                focusTarget_ = FocusTarget::CloseButton;
                return;
            }
            if (focusTarget_ == FocusTarget::CloseButton) {
                focusTarget_ = FocusTarget::ExitButton;
                return;
            }
            if (count > 0) {
                setSelectedSettingRow(0);
            }
            return;
        }
        if (focusTarget_ == FocusTarget::ApplyAction ||
            focusTarget_ == FocusTarget::ResetWorldAction ||
            focusTarget_ == FocusTarget::ResetControlsAction)
        {
            if (focusTarget_ == FocusTarget::ResetWorldAction && hasPendingSettingsChanges()) {
                focusTarget_ = FocusTarget::ApplyAction;
            }
            return;
        }

        discardPendingEdit();
        if (selectedSettingRow_ >= count - 1) {
            if (settingsPage_ == SettingsPage::Simulation) {
                focusTarget_ = FocusTarget::ResetWorldAction;
            } else if (settingsPage_ == SettingsPage::Controls && controlsDirty_) {
                focusTarget_ = FocusTarget::ResetControlsAction;
            } else if (hasPendingSettingsChanges()) {
                focusTarget_ = FocusTarget::ApplyAction;
            }
        } else {
            setSelectedSettingRow(selectedSettingRow_ + 1);
        }
        statusMessage_.clear();
    }

    void PauseMenuController::selectPrev() {
        const int count = settingCount();
        if (hasOpenPopup()) {
            if (focusTarget_ != FocusTarget::PopupYes && focusTarget_ != FocusTarget::PopupNo) {
                focusTarget_ = FocusTarget::PopupYes;
            }
            return;
        }
        if (count <= 0 && focusTarget_ == FocusTarget::SettingsRow) {
            return;
        }

        if (focusTarget_ == FocusTarget::ApplyAction ||
            focusTarget_ == FocusTarget::ResetWorldAction ||
            focusTarget_ == FocusTarget::ResetControlsAction)
        {
            if (focusTarget_ == FocusTarget::ApplyAction && settingsPage_ == SettingsPage::Simulation) {
                focusTarget_ = FocusTarget::ResetWorldAction;
                return;
            }
            if (count > 0) {
                setSelectedSettingRow(count - 1);
            }
            return;
        }
        if (focusTarget_ == FocusTarget::CloseButton ||
            focusTarget_ == FocusTarget::ExitButton ||
            focusTarget_ == FocusTarget::ResetIcon)
        {
            if (focusTarget_ == FocusTarget::ExitButton) {
                focusTarget_ = FocusTarget::CloseButton;
                return;
            }
            if (focusTarget_ == FocusTarget::CloseButton) {
                focusTarget_ = FocusTarget::ResetIcon;
                return;
            }
            return;
        }

        discardPendingEdit();
        if (selectedSettingRow_ <= 0) {
            focusTarget_ = FocusTarget::ExitButton;
        } else {
            setSelectedSettingRow(selectedSettingRow_ - 1);
        }
        statusMessage_.clear();
    }

    void PauseMenuController::selectLeft(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        (void)window;
        (void)controls;
        (void)controlsConfigPath;
        if (hasOpenPopup()) {
            focusTarget_ = FocusTarget::PopupYes;
            return;
        }
        switch (focusTarget_) {
            case FocusTarget::ResetControlsAction:
                return;
            case FocusTarget::SettingsRow:
                if (selectedSettingRow_ >= 0 && !isControlPage()) {
                    adjustSelectedSetting(-1);
                }
                return;
            default:
                return;
        }
    }

    void PauseMenuController::selectRight(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        (void)window;
        (void)controls;
        (void)controlsConfigPath;
        if (hasOpenPopup()) {
            focusTarget_ = popupUsesSingleAcknowledge() ? FocusTarget::PopupYes : FocusTarget::PopupNo;
            return;
        }
        switch (focusTarget_) {
            case FocusTarget::ResetControlsAction:
                return;
            case FocusTarget::SettingsRow:
                if (selectedSettingRow_ >= 0 && !isControlPage()) {
                    adjustSelectedSetting(1);
                }
                return;
            default:
                return;
        }
    }

    void PauseMenuController::activateFocusedControl(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        if (popupType_ == PopupType::EnableDrawPath && focusTarget_ == FocusTarget::PopupYes) {
            ensureDraftForSelectedRow();
            draftInterfaceSettings_.drawPath = true;
            pendingSelectionEdit_ = hasPendingSelectionChanges();
            if (!pendingSelectionEdit_) {
                draftSelectionRow_ = -1;
                draftSettingsPage_ = settingsPage_;
            }
            statusMessage_.clear();
            closePopup();
            return;
        }
        if (popupType_ == PopupType::ResetWorld && focusTarget_ == FocusTarget::PopupYes) {
            resetWorldRequested_ = true;
            statusMessage_ = "WORLD RESET";
            closePopup(false);
            return;
        }
        if (popupType_ == PopupType::ExitToHome && focusTarget_ == FocusTarget::PopupYes) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            closePopup(false);
            return;
        }
        if (popupType_ == PopupType::ResetSettings && focusTarget_ == FocusTarget::PopupYes) {
            resetAllSettings(window);
            closePopup(false);
            return;
        }
        if (hasOpenPopup() && focusTarget_ == FocusTarget::PopupNo)
        {
            closePopup();
            return;
        }
        switch (focusTarget_) {
            case FocusTarget::ResetIcon:
                openPopup(PopupType::ResetSettings, FocusTarget::ResetIcon, true);
                return;
            case FocusTarget::CloseButton:
                resumeRequested_ = true;
                return;
            case FocusTarget::ExitButton:
                openPopup(PopupType::ExitToHome, FocusTarget::ExitButton, true);
                return;
            case FocusTarget::ApplyAction:
                commitSelectedSetting(window);
                return;
            case FocusTarget::ResetWorldAction:
                openPopup(PopupType::ResetWorld, FocusTarget::ResetWorldAction, true);
                return;
            case FocusTarget::ResetControlsAction:
                controls = input::ControlBindings{};
                controlsDirty_ = false;
                input::saveControlBindings(controls, controlsConfigPath);
                markAppliedSelection();
                statusMessage_.clear();
                return;
            case FocusTarget::SettingsRow:
                if (selectedSettingRow_ < 0) {
                    return;
                }
                if (isControlPage()) {
                    beginRebindForRow(selectedSettingRow_);
                } else {
                    commitSelectedSetting(window);
                }
                return;
            case FocusTarget::PopupYes:
            case FocusTarget::PopupNo:
                return;
        }
    }

    bool PauseMenuController::isSettingRowDisabled(const int row) const {
        if (row < 0) {
            return true;
        }
        if (settingsPage_ == SettingsPage::Display) {
            return false;
        }
        if (settingsPage_ == SettingsPage::Interface) {
            const InterfaceSettings& settings =
                (pendingSelectionEdit_ && draftSettingsPage_ == SettingsPage::Interface)
                    ? draftInterfaceSettings_
                    : appliedInterfaceSettings_;
            if (row == 3 && !settings.showHud) {
                return true;
            }
            return row >= 6 && row <= 12 && !settings.objectInfo;
        }
        return false;
    }

    std::string PauseMenuController::disabledReasonForRow(const int row) const {
        if (settingsPage_ == SettingsPage::Interface && row == 3) {
            return "ENABLE SHOW HUD FIRST";
        }
        if (settingsPage_ == SettingsPage::Interface && row >= 6 && row <= 12) {
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
            if (row == 2 || row == 4) {
                return OverlayRenderer::PauseMenuControlType::Toggle;
            }
            return row <= 7 ? OverlayRenderer::PauseMenuControlType::Numeric : OverlayRenderer::PauseMenuControlType::None;
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
            if (row <= 12) {
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
                case 4: return 2;
                case 5: return static_cast<int>(kCollisionIterationChoices.size());
                case 6: return static_cast<int>(kGlobalRestitutionChoices.size());
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
            if (row >= 1 && row <= 11) {
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
            const SimulationSettings& settings =
                (pendingSelectionEdit_ && draftSettingsPage_ == SettingsPage::Simulation)
                    ? draftSimulationSettings_
                    : appliedSimulationSettings_;
            switch (row) {
                case 0: return closestChoiceIndex(kMinSpeedChoices, settings.minSimSpeed);
                case 1: return closestChoiceIndex(kMaxSpeedChoices, settings.maxSimSpeed);
                case 2: return settings.gravityEnabled ? 1 : 0;
                case 3: return closestChoiceIndex(kGravityStrengthChoices, settings.gravityStrength);
                case 4: return settings.collisionsEnabled ? 1 : 0;
                case 5: return closestChoiceIndex(kCollisionIterationChoices, settings.collisionIterations);
                case 6: return closestChoiceIndex(kGlobalRestitutionChoices, settings.globalRestitution);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Camera) {
            const CameraSettings& settings =
                (pendingSelectionEdit_ && draftSettingsPage_ == SettingsPage::Camera)
                    ? draftCameraSettings_
                    : appliedCameraSettings_;
            switch (row) {
                case 0: return closestChoiceIndex(kLookSensitivityChoices, settings.lookSensitivity);
                case 1: return closestChoiceIndex(kBaseMoveSpeedChoices, settings.baseMoveSpeed);
                case 2: return settings.invertY ? 1 : 0;
                case 3: return closestChoiceIndex(kFovChoices, settings.fovDegrees);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Interface) {
            const InterfaceSettings& settings =
                (pendingSelectionEdit_ && draftSettingsPage_ == SettingsPage::Interface)
                    ? draftInterfaceSettings_
                    : appliedInterfaceSettings_;
            switch (row) {
                case 0:
                    return std::clamp(settings.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
                case 1: return settings.showHud ? 1 : 0;
                case 2: return settings.showCrosshair ? 1 : 0;
                case 3: return settings.showPhysicsStats ? 1 : 0;
                case 4: return settings.drawPath ? 1 : 0;
                case 5: return settings.objectInfo ? 1 : 0;
                case 6: return settings.objectInfoMaterial ? 1 : 0;
                case 7: return settings.objectInfoVelocity ? 1 : 0;
                case 8: return settings.objectInfoMass ? 1 : 0;
                case 9: return settings.objectInfoRadius ? 1 : 0;
                case 10: return settings.objectInfoAngularSpeed ? 1 : 0;
                case 11: return settings.objectInfoBodyType ? 1 : 0;
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
                    settings.collisionsEnabled = !settings.collisionsEnabled;
                    break;
                }
                case 5: {
                    const int idx = std::clamp(closestChoiceIndex(kCollisionIterationChoices, settings.collisionIterations) + delta, 0, static_cast<int>(kCollisionIterationChoices.size()) - 1);
                    settings.collisionIterations = kCollisionIterationChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 6: {
                    const int idx = std::clamp(closestChoiceIndex(kGlobalRestitutionChoices, settings.globalRestitution) + delta, 0, static_cast<int>(kGlobalRestitutionChoices.size()) - 1);
                    settings.globalRestitution = kGlobalRestitutionChoices[static_cast<std::size_t>(idx)];
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
                    settings.showPhysicsStats = !settings.showPhysicsStats;
                    break;
                case 4:
                    if (!settings.drawPath) {
                        openPopup(PopupType::EnableDrawPath, FocusTarget::SettingsRow, true);
                        statusMessage_.clear();
                        return;
                    }
                    settings.drawPath = false;
                    break;
                case 5:
                    settings.objectInfo = !settings.objectInfo;
                    break;
                case 6:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMaterial = !settings.objectInfoMaterial;
                    break;
                case 7:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoVelocity = !settings.objectInfoVelocity;
                    break;
                case 8:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMass = !settings.objectInfoMass;
                    break;
                case 9:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoRadius = !settings.objectInfoRadius;
                    break;
                case 10:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoAngularSpeed = !settings.objectInfoAngularSpeed;
                    break;
                case 11:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoBodyType = !settings.objectInfoBodyType;
                    break;
                default:
                    break;
            }
        }

        statusMessage_.clear();
        pendingSelectionEdit_ = hasPendingSelectionChanges();
        if (!pendingSelectionEdit_) {
            draftSelectionRow_ = -1;
            draftSettingsPage_ = settingsPage_;
        }
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

    bool PauseMenuController::consumeResetWorldRequest() {
        const bool requested = resetWorldRequested_;
        resetWorldRequested_ = false;
        return requested;
    }

    bool PauseMenuController::hasPendingDisplayChanges() const {
        return draftDisplaySettings_.windowMode != appliedDisplaySettings_.windowMode ||
               draftDisplaySettings_.vsync != appliedDisplaySettings_.vsync;
    }

    bool PauseMenuController::hasPendingSimulationChanges() const {
        return draftSimulationSettings_.minSimSpeed != appliedSimulationSettings_.minSimSpeed ||
               draftSimulationSettings_.maxSimSpeed != appliedSimulationSettings_.maxSimSpeed ||
               draftSimulationSettings_.gravityEnabled != appliedSimulationSettings_.gravityEnabled ||
               draftSimulationSettings_.gravityStrength != appliedSimulationSettings_.gravityStrength ||
               draftSimulationSettings_.collisionsEnabled != appliedSimulationSettings_.collisionsEnabled ||
               draftSimulationSettings_.collisionIterations != appliedSimulationSettings_.collisionIterations ||
               draftSimulationSettings_.globalRestitution != appliedSimulationSettings_.globalRestitution;
    }

    bool PauseMenuController::hasPendingCameraChanges() const {
        return draftCameraSettings_.lookSensitivity != appliedCameraSettings_.lookSensitivity ||
               draftCameraSettings_.baseMoveSpeed != appliedCameraSettings_.baseMoveSpeed ||
               draftCameraSettings_.invertY != appliedCameraSettings_.invertY ||
               draftCameraSettings_.fovDegrees != appliedCameraSettings_.fovDegrees;
    }

    bool PauseMenuController::hasPendingInterfaceChanges() const {
        return draftInterfaceSettings_.uiScaleIndex != appliedInterfaceSettings_.uiScaleIndex ||
               draftInterfaceSettings_.showHud != appliedInterfaceSettings_.showHud ||
               draftInterfaceSettings_.showCrosshair != appliedInterfaceSettings_.showCrosshair ||
               draftInterfaceSettings_.showPhysicsStats != appliedInterfaceSettings_.showPhysicsStats ||
               draftInterfaceSettings_.drawPath != appliedInterfaceSettings_.drawPath ||
               draftInterfaceSettings_.objectInfo != appliedInterfaceSettings_.objectInfo ||
               draftInterfaceSettings_.objectInfoMaterial != appliedInterfaceSettings_.objectInfoMaterial ||
               draftInterfaceSettings_.objectInfoVelocity != appliedInterfaceSettings_.objectInfoVelocity ||
               draftInterfaceSettings_.objectInfoMass != appliedInterfaceSettings_.objectInfoMass ||
               draftInterfaceSettings_.objectInfoRadius != appliedInterfaceSettings_.objectInfoRadius ||
               draftInterfaceSettings_.objectInfoAngularSpeed != appliedInterfaceSettings_.objectInfoAngularSpeed ||
               draftInterfaceSettings_.objectInfoBodyType != appliedInterfaceSettings_.objectInfoBodyType;
    }

    bool PauseMenuController::hasPendingSelectionChanges() const {
        if (!pendingSelectionEdit_) {
            return false;
        }

        switch (draftSettingsPage_) {
            case SettingsPage::Simulation:
                return hasPendingSimulationChanges();
            case SettingsPage::Camera:
                return hasPendingCameraChanges();
            case SettingsPage::Interface:
                return hasPendingInterfaceChanges();
            default:
                return false;
        }
    }

    bool PauseMenuController::hasPendingSettingsChanges() const {
        if (isControlPage()) {
            return false;
        }
        if (settingsPage_ == SettingsPage::Display) {
            return pendingDisplayChanges_;
        }
        return pendingSelectionEdit_ &&
               draftSettingsPage_ == settingsPage_ &&
               draftSelectionRow_ == selectedSettingRow_ &&
               hasPendingSelectionChanges();
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
        appliedSimulationSettings_.globalRestitution =
            kGlobalRestitutionChoices[static_cast<std::size_t>(closestChoiceIndex(kGlobalRestitutionChoices, appliedSimulationSettings_.globalRestitution))];
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

#ifndef PHYSICS3D_PAUSEMENUCONTROLLER_H
#define PHYSICS3D_PAUSEMENUCONTROLLER_H

#include <array>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "sim/Body.h"
#include "sim/World.h"
#include "ui/OverlayRenderer.h"

struct GLFWwindow;

namespace ui {
    enum class WindowMode {
        Borderless = 0,
        Windowed = 1,
    };

    struct DisplaySettings {
        WindowMode windowMode = WindowMode::Borderless;
        bool vsync = true;
        int windowedX = 120;
        int windowedY = 120;
        int windowedWidth = 1920;
        int windowedHeight = 1080;
    };

    struct SimulationSettings {
        double minSimSpeed = 1.0 / 64.0;
        double maxSimSpeed = 64.0;
        bool gravityEnabled = true;
        double gravityStrength = sim::World::Params::kDefaultG;
        bool collisionsEnabled = true;
        int collisionIterations = 2;
        double globalRestitution = sim::World::Params::kDefaultRestitution;
    };

    struct CameraSettings {
        float lookSensitivity = 0.0025f;
        float baseMoveSpeed = 40.0f;
        bool invertY = false;
        float fovDegrees = 60.0f;
    };

    struct InterfaceSettings {
        int uiScaleIndex = 2; // 1.00x
        bool showHud = true;
        bool showCrosshair = true;
        bool showPhysicsStats = false;
        bool drawPath = false;
        bool objectInfo = true;
        bool objectInfoMaterial = false;
        bool objectInfoVelocity = true;
        bool objectInfoMass = true;
        bool objectInfoRadius = true;
        bool objectInfoAngularSpeed = false;
        bool objectInfoBodyType = false;
    };

    enum class SettingsPage {
        Display = 0,
        Simulation = 1,
        Camera = 2,
        Interface = 3,
        Controls = 4,
    };

    class PauseMenuController {
    public:
        bool isOpen() const { return open_; }
        const DisplaySettings& displaySettings() const { return appliedDisplaySettings_; }
        const SimulationSettings& simulationSettings() const { return appliedSimulationSettings_; }
        const CameraSettings& cameraSettings() const { return appliedCameraSettings_; }
        const InterfaceSettings& interfaceSettings() const { return appliedInterfaceSettings_; }

        float uiScale() const;
        void loadSettings(const std::string& path);
        void applyCurrentDisplaySettings(GLFWwindow* window);

        void updateEscapeState(
            GLFWwindow* window,
            bool& mouseCaptured,
            bool& firstMouse,
            double& lastTime,
            double& accumulator,
            std::vector<sim::Body>& bodies);

        void handlePressedKey(
            GLFWwindow* window,
            int pressedKey,
            input::ControlBindings& controls,
            const std::string& controlsConfigPath);
        void handlePointerInput(
            GLFWwindow* window,
            input::ControlBindings& controls,
            const std::string& controlsConfigPath,
            float scrollDeltaY = 0.0f);
        void updateContinuousInput(
            GLFWwindow* window,
            const input::ControlBindings& controls,
            const std::string& controlsConfigPath);

        OverlayRenderer::PauseMenuHud buildHud(
            const input::ControlBindings& controls,
            bool advanceApplyIndicator = true) const;
        bool consumeResetWorldRequest();

    private:
        enum class FocusTarget {
            SettingsRow = 0,
            ResetIcon,
            CloseButton,
            ExitButton,
            ApplyAction,
            ResetWorldAction,
            ResetControlsAction,
            PopupYes,
            PopupNo,
        };

        enum class PopupType {
            None = 0,
            ResetSettings,
            ResetWorld,
            ExitToHome,
            EnableDrawPath,
        };

        bool open_ = false;
        bool awaitingRebind_ = false;
        int awaitingRebindAction_ = -1;
        SettingsPage settingsPage_ = SettingsPage::Display;
        int selectedSettingRow_ = -1;
        SettingsPage draftSettingsPage_ = SettingsPage::Display;
        int draftSelectionRow_ = -1;
        bool pendingSelectionEdit_ = false;
        bool pendingDisplayChanges_ = false;
        bool controlsDirty_ = false;
        std::string statusMessage_;
        FocusTarget focusTarget_ = FocusTarget::SettingsRow;
        FocusTarget popupReturnTarget_ = FocusTarget::SettingsRow;
        mutable SettingsPage lastAppliedPage_ = SettingsPage::Display;
        mutable int lastAppliedRow_ = -1;
        mutable int applyIndicatorFrames_ = 0;
        bool escWasDown_ = false;
        bool backWasDown_ = false;
        bool resumeRequested_ = false;
        bool resetWorldRequested_ = false;
        bool leftMouseWasDown_ = false;
        bool ignoreNextRebindMousePress_ = false;
        int hoveredSettingRow_ = -1;
        bool hoveredDisplayApplyAction_ = false;
        bool hoveredResetWorldAction_ = false;
        bool hoveredResetControlsAction_ = false;
        bool hoveredResetIcon_ = false;
        PopupType popupType_ = PopupType::None;
        bool hoveredResetConfirmYes_ = false;
        bool hoveredResetConfirmNo_ = false;
        SettingsPage lastClickedPage_ = SettingsPage::Display;
        int lastClickedRow_ = -1;
        double lastClickAt_ = -1.0;
        std::array<bool, input::kMouseBindingMaxButtons> mouseButtonWasDown_{};
        bool upHeld_ = false;
        bool downHeld_ = false;
        bool leftHeld_ = false;
        bool rightHeld_ = false;
        double upNextRepeatAt_ = 0.0;
        double downNextRepeatAt_ = 0.0;
        double leftNextRepeatAt_ = 0.0;
        double rightNextRepeatAt_ = 0.0;
        int scrollLineOffset_ = 0;
        DisplaySettings appliedDisplaySettings_{};
        DisplaySettings draftDisplaySettings_{};
        SimulationSettings appliedSimulationSettings_{};
        SimulationSettings draftSimulationSettings_{};
        CameraSettings appliedCameraSettings_{};
        CameraSettings draftCameraSettings_{};
        InterfaceSettings appliedInterfaceSettings_{};
        InterfaceSettings draftInterfaceSettings_{};
        std::string settingsConfigPath_;

        int settingCount() const;
        static int controlCount();
        static int displaySettingRowCount();
        static int simulationSettingRowCount();
        static int cameraSettingRowCount();
        static int interfaceSettingRowCount();
        static float uiScaleValue(int idx);
        static const char* windowModeLabel(WindowMode mode);
        static int wrapSelectionRow(int row, int count);
        static const char* settingsPageLabel(SettingsPage page);
        static std::string formatMenuValue(const char* value);
        static std::string formatMenuValue(const std::string& value);

        bool isControlPage() const;
        void setSelectedSettingRow(int row);
        void setFocusTarget(FocusTarget target);
        void discardPendingEdit();
        void ensureDraftForSelectedRow();
        void switchPage(int delta);
        void switchToPage(SettingsPage page);
        void commitSelectedSetting(GLFWwindow* window);
        bool beginRebindForRow(int row);
        bool applyRebindCode(int code, input::ControlBindings& controls, const std::string& controlsConfigPath);
        void selectNext();
        void selectPrev();
        void selectLeft(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
        void selectRight(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
        void activateFocusedControl(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
        bool isSettingRowDisabled(int row) const;
        std::string disabledReasonForRow(int row) const;
        OverlayRenderer::PauseMenuControlType controlTypeForRow(int row) const;
        bool supportsContinuousAdjust(int row) const;
        int optionCountForRow(int row) const;
        int optionIndexForRow(int row) const;
        bool setOptionIndexForRow(int row, int targetIndex);
        void adjustSelectedSetting(int delta);
        void applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings);
        void resetAllSettings(GLFWwindow* window);
        bool hasOpenPopup() const;
        bool popupUsesSingleAcknowledge() const;
        void openPopup(PopupType type, FocusTarget returnTarget, bool selectDefaultAction);
        void closePopup(bool restoreFocus = true);
        static bool hasControlChanges(const input::ControlBindings& controls);
        bool hasPendingDisplayChanges() const;
        bool hasPendingSimulationChanges() const;
        bool hasPendingCameraChanges() const;
        bool hasPendingInterfaceChanges() const;
        bool hasPendingSelectionChanges() const;
        bool hasPendingSettingsChanges() const;
        void normalizeAppliedSettings();
        void saveSettings() const;
        void markAppliedSelection() const;
    };
}

#endif //PHYSICS3D_PAUSEMENUCONTROLLER_H

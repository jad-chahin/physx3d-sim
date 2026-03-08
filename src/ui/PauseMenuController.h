//
// Created by Codex on 2026-03-07.
//

#ifndef PHYSICS3D_PAUSEMENUCONTROLLER_H
#define PHYSICS3D_PAUSEMENUCONTROLLER_H

#include <string>
#include <vector>

#include "input/Bindings.h"
#include "sim/Body.h"
#include "ui/DebugOverlay.h"

struct GLFWwindow;

namespace ui {
    enum class WindowMode {
        Borderless = 0,
        Windowed = 1,
        Fullscreen = 2,
    };

    struct DisplaySettings {
        WindowMode windowMode = WindowMode::Borderless;
        bool vsync = false;
        int resolutionIndex = 0; // 0 = default monitor resolution
    };

    struct SimulationSettings {
        double minSimSpeed = 1.0 / 128.0;
        double maxSimSpeed = 128.0;
        int maxPhysicsStepsPerFrame = 10;
    };

    struct CameraSettings {
        float lookSensitivity = 0.0025f;
        float baseMoveSpeed = 40.0f;
        bool invertY = false;
        float fovDegrees = 60.0f;
    };

    struct InterfaceSettings {
        int uiScaleIndex = 1; // 1.00x
        bool showHud = true;
        bool objectInfo = true;
        bool objectInfoMaterial = true;
        bool objectInfoVelocity = true;
        bool objectInfoMass = true;
        bool objectInfoRadius = true;
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
        void handlePointerInput(GLFWwindow* window, const input::ControlBindings& controls);

        DebugOverlay::PauseMenuHud buildHud(
            const input::ControlBindings& controls,
            bool advanceApplyIndicator = true) const;

    private:
        bool open_ = false;
        bool awaitingRebind_ = false;
        int awaitingRebindAction_ = -1;
        SettingsPage settingsPage_ = SettingsPage::Display;
        int selectedSettingRow_ = 0;
        SettingsPage draftSettingsPage_ = SettingsPage::Display;
        int draftSelectionRow_ = -1;
        bool pendingSelectionEdit_ = false;
        std::string statusMessage_;
        mutable SettingsPage lastAppliedPage_ = SettingsPage::Display;
        mutable int lastAppliedRow_ = -1;
        mutable int applyIndicatorFrames_ = 0;
        bool escWasDown_ = false;
        bool leftMouseWasDown_ = false;
        int windowedX_ = 120;
        int windowedY_ = 120;
        int windowedWidth_ = 1920;
        int windowedHeight_ = 1080;
        DisplaySettings appliedDisplaySettings_{};
        DisplaySettings draftDisplaySettings_{};
        SimulationSettings appliedSimulationSettings_{};
        SimulationSettings draftSimulationSettings_{};
        CameraSettings appliedCameraSettings_{};
        CameraSettings draftCameraSettings_{};
        InterfaceSettings appliedInterfaceSettings_{};
        InterfaceSettings draftInterfaceSettings_{};

        int settingCount() const;
        static int controlCount();
        static const char* resolutionLabel(int idx);
        static int resolutionWidth(int idx);
        static int resolutionHeight(int idx);
        static int resolutionChoiceCount();
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
        void discardPendingEdit();
        void ensureDraftForSelectedRow();
        void switchPage(int delta);
        void switchToPage(SettingsPage page);
        void commitSelectedSetting(GLFWwindow* window);
        void selectNext();
        void selectPrev();
        void adjustSelectedSetting(int delta);
        void applyDisplaySettings(GLFWwindow* window, const DisplaySettings& settings);
        void markAppliedSelection() const;
    };
}

#endif //PHYSICS3D_PAUSEMENUCONTROLLER_H

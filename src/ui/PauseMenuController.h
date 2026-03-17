#ifndef PHYSICS3D_PAUSEMENUCONTROLLER_H
#define PHYSICS3D_PAUSEMENUCONTROLLER_H

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "input/Bindings.h"
#include "sim/Body.h"
#include "sim/World.h"
#include "ui/OverlayRenderer.h"

struct GLFWwindow;

namespace ui::testing {
    class PauseMenuControllerAccess;
}

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

        bool operator==(const DisplaySettings&) const = default;
    };

    struct SimulationSettings {
        double minSimSpeed = 1.0 / 64.0;
        double maxSimSpeed = 64.0;
        bool gravityEnabled = true;
        double gravityStrength = sim::World::Params::kDefaultG;
        bool collisionsEnabled = true;
        int velocityIterations = sim::World::Params::kDefaultVelocityIterations;
        int positionIterations = sim::World::Params::kDefaultPositionIterations;
        double globalRestitution = sim::World::Params::kDefaultRestitution;

        bool operator==(const SimulationSettings&) const = default;
    };

    struct CameraSettings {
        float lookSensitivity = 0.0025f;
        float baseMoveSpeed = 40.0f;
        bool invertY = false;
        float fovDegrees = 60.0f;

        bool operator==(const CameraSettings&) const = default;
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

        bool operator==(const InterfaceSettings&) const = default;
    };

    struct PauseMenuSettingsBundle {
        DisplaySettings display{};
        SimulationSettings simulation{};
        CameraSettings camera{};
        InterfaceSettings interface{};

        bool operator==(const PauseMenuSettingsBundle&) const = default;
    };

    enum class SettingsPage {
        Display = 0,
        Simulation = 1,
        Camera = 2,
        Interface = 3,
        Controls = 4,
    };

    namespace pause_menu_shared {
        inline constexpr int kFontH = 7;
        inline constexpr int kAdvance = 6;
        inline constexpr float kBaseScalePx = 1.90f;
        inline constexpr float kSettingsScalePx = 2.00f;
        inline constexpr float kTitleScalePx = 2.85f;

        inline constexpr std::array<float, 7> kUiScaleChoices{{
            0.75f, 0.85f, 1.00f, 1.15f, 1.30f, 1.40f, 1.50f,
        }};

        inline constexpr std::array<double, 9> kMinSpeedChoices{{
            1.0 / 256.0, 1.0 / 128.0, 1.0 / 64.0, 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0,
        }};

        inline constexpr std::array<double, 9> kMaxSpeedChoices{{
            1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0,
        }};

        inline constexpr double kDefaultGravityStrength = 6.6743e-11;
        inline constexpr std::array<double, 9> kGravityStrengthChoices{{
            kDefaultGravityStrength * 0.125, kDefaultGravityStrength * 0.25, kDefaultGravityStrength * 0.50,
            kDefaultGravityStrength * 0.75,  kDefaultGravityStrength * 1.00, kDefaultGravityStrength * (4.0 / 3.0),
            kDefaultGravityStrength * 2.00,  kDefaultGravityStrength * 4.00, kDefaultGravityStrength * 8.00,
        }};

        inline constexpr std::array<int, 8> kCollisionIterationChoices{{1, 2, 3, 4, 5, 6, 8, 10}};
        inline constexpr std::array<double, 11> kGlobalRestitutionChoices{{
            0.00, 0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00,
        }};

        inline constexpr std::array<float, 9> kLookSensitivityChoices{{
            0.0008f, 0.0012f, 0.0016f, 0.0020f, 0.0025f, 0.0030f, 0.0038f, 0.0048f, 0.0060f,
        }};

        inline constexpr std::array<float, 7> kBaseMoveSpeedChoices{{
            10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f,
        }};

        inline constexpr std::array<float, 8> kFovChoices{{
            50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f,
        }};

        inline constexpr std::array<const char*, 5> kPageTabLabels{{
            "DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS",
        }};

        template <typename T, std::size_t N>
        int closestChoiceIndex(const std::array<T, N>& choices, const T value) {
            int bestIdx = 0;
            auto bestDiff = static_cast<double>(choices[0] - value);
            bestDiff = bestDiff < 0.0 ? -bestDiff : bestDiff;
            for (std::size_t i = 1; i < N; ++i) {
                auto diff = static_cast<double>(choices[i] - value);
                diff = diff < 0.0 ? -diff : diff;
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestIdx = static_cast<int>(i);
                }
            }
            return bestIdx;
        }

        struct Rect {
            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;

            [[nodiscard]] bool contains(float x, float y) const {
                return x >= x0 && x <= x1 && y >= y0 && y <= y1;
            }
        };

        struct PauseMenuLayout {
            struct LineWidgets {
                Rect line{};
                Rect value{};
                Rect checkbox{};
                Rect leftArrow{};
                Rect rightArrow{};
                Rect slider{};
                bool hasValue = false;
                bool hasCheckbox = false;
                bool hasLeftArrow = false;
                bool hasRightArrow = false;
                bool hasSlider = false;
            };

            float width = 0.0f;
            float height = 0.0f;
            float menuScale = 1.0f;
            float baseScalePx = 0.0f;
            float rowScalePx = 0.0f;
            float tabsScalePx = 0.0f;
            float sectionHeaderScalePx = 0.0f;
            float actionScalePx = 0.0f;
            float actionButtonH = 0.0f;
            float cardX0 = 0.0f;
            float cardY0 = 0.0f;
            float cardX1 = 0.0f;
            float cardY1 = 0.0f;
            float sectionX0 = 0.0f;
            float sectionX1 = 0.0f;
            float settingsY0 = 0.0f;
            float settingsY1 = 0.0f;
            float linesStartY = 0.0f;
            float rowStep = 0.0f;
            float contentY1 = 0.0f;
            std::size_t totalLines = 0;
            std::size_t visibleLines = 0;
            std::size_t firstLine = 0;
            std::array<Rect, kPageTabLabels.size()> tabRects{};
            std::array<Rect, static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Count)> actionRects{};
            Rect popupRect{};

            [[nodiscard]] Rect lineRect(std::size_t visibleIndex) const;
            [[nodiscard]] const Rect& actionRect(OverlayRenderer::PauseMenuAction action) const;
            [[nodiscard]] LineWidgets lineWidgets(std::size_t visibleIndex, const OverlayRenderer::PauseMenuHudLine& line) const;
        };

        struct PopupButtonsLayout {
            bool singleAcknowledge = false;
            float buttonScalePx = 0.0f;
            float buttonPadX = 0.0f;
            float buttonPadY = 0.0f;
            Rect yesButton{};
            Rect noButton{};
            std::string yesLabel;
            std::string noLabel;
        };

        [[nodiscard]] std::string formatSpeedMultiplier(double value);
        [[nodiscard]] std::string formatGravityMultiplier(double value);
        [[nodiscard]] std::string formatRestitutionPercent(double value);
        [[nodiscard]] PauseMenuLayout buildPauseMenuLayout(float width, float height, float uiScale, const OverlayRenderer::PauseMenuHud& hud);
        [[nodiscard]] PopupButtonsLayout buildPopupButtonsLayout(const PauseMenuLayout& layout, const OverlayRenderer::PauseMenuHud& hud);
    } // namespace pause_menu_shared

    class PauseMenuController {
    public:
        friend class testing::PauseMenuControllerAccess;

        bool isOpen() const { return open_; }
        const DisplaySettings& displaySettings() const { return appliedSettings_.display; }
        const SimulationSettings& simulationSettings() const { return appliedSettings_.simulation; }
        const CameraSettings& cameraSettings() const { return appliedSettings_.camera; }
        const InterfaceSettings& interfaceSettings() const { return appliedSettings_.interface; }

        float uiScale() const;
        void loadSettings(const std::string& path);
        void applyCurrentDisplaySettings(GLFWwindow* window);

        void updateEscapeState(
            GLFWwindow* window,
            bool& mouseCaptured,
            bool& firstMouse,
            double& lastFrameTime,
            double& accumulator,
            double& alpha,
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
            const input::ControlBindings& controls);

        OverlayRenderer::PauseMenuHud buildHud(
            const input::ControlBindings& controls,
            bool advanceApplyIndicator = true) const;
        bool consumeResetWorldRequest();

    private:
        // Shared field IDs keep HUD/state/input logic keyed off one model.
    public:
        enum class SettingField {
            WindowMode,
            Vsync,
            MinSpeed,
            MaxSpeed,
            Gravity,
            GravityStrength,
            Collisions,
            VelocityIterations,
            PositionIterations,
            GlobalBounce,
            LookSensitivity,
            BaseMoveSpeed,
            InvertY,
            Fov,
            UiScale,
            ShowHud,
            Crosshair,
            DebugStats,
            DrawPath,
            ObjectInfo,
            ObjectInfoMaterial,
            ObjectInfoVelocity,
            ObjectInfoMass,
            ObjectInfoRadius,
            ObjectInfoAngularSpeed,
            ObjectInfoBodyType,
        };

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

        enum class PopupConfirmAction {
            None = 0,
            ResetSettings,
            ResetWorld,
            ExitToHome,
            EnableDrawPath,
        };

        struct PopupModel {
            const char* titleText = "";
            const char* bodyText = "";
            const char* yesLabel = "";
            const char* noLabel = "";
            bool singleAcknowledge = false;
            bool keepSelectedLine = false;
            bool restoreFocusOnConfirm = true;
            PopupConfirmAction confirmAction = PopupConfirmAction::None;
        };

        bool open_ = false;
        bool awaitingRebind_ = false;
        int awaitingRebindAction_ = -1;
        SettingsPage settingsPage_ = SettingsPage::Display;
        int selectedSettingRow_ = -1;
        SettingsPage draftSettingsPage_ = SettingsPage::Display;
        int draftSelectionRow_ = -1;
        bool pendingSelectionEdit_ = false;
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
        std::array<bool, static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Count)> hoveredActions_{};
        int hoveredPageTabIndex_ = -1;
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
        PauseMenuSettingsBundle appliedSettings_{};
        PauseMenuSettingsBundle draftSettings_{};
        std::string settingsConfigPath_;

        int settingCount() const;
        static std::span<const SettingField> settingFieldsForPage(SettingsPage page);
        SettingField settingFieldForRow(int row) const;
        static int controlCount();
        static float uiScaleValue(int idx);

        bool isControlPage() const;
        void clearHoverState();
        const PauseMenuSettingsBundle& activeSettings() const;
        void setSelectedSettingRow(int row);
        void restoreDraftFromApplied();
        void discardPendingEdit();
        void ensureDraftForSelectedRow();
        void switchPage(int delta);
        void switchToPage(SettingsPage page);
        void commitSelectedSetting(GLFWwindow* window);
        bool beginRebindForRow(int row);
        bool applyRebindCode(int code, input::ControlBindings& controls, const std::string& controlsConfigPath);
        void moveVerticalFocus(int delta, bool controlsDirty);
        void moveHorizontalFocus(int delta);
        void resetControlsToDefaults(input::ControlBindings& controls, const std::string& controlsConfigPath);
        void activateFocusedControl(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
        bool isSettingRowDisabled(int row) const;
        std::string disabledReasonForRow(int row) const;
        void adjustSelectedSetting(int delta, bool selectPopupDefaultAction = true);

        static void applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings);
        void resetAllSettings(GLFWwindow* window);
        bool hasOpenPopup() const;
        PopupModel popupModel() const;
        void openPopup(PopupType type, FocusTarget returnTarget, bool selectDefaultAction);
        void closePopup(bool restoreFocus = true);
        void confirmPopup(GLFWwindow* window);
        bool hasPendingPageChanges(SettingsPage page) const;
        bool hasPendingSelectionChanges() const;
        bool hasPendingSettingsChanges() const;
        void normalizeAppliedSettings();
        void saveSettings() const;
        void markAppliedSelection() const;
    };
} // namespace ui

#endif // PHYSICS3D_PAUSEMENUCONTROLLER_H

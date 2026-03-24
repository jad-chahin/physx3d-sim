#ifndef PHYSICS3D_UI_PAUSEMENU_H
#define PHYSICS3D_UI_PAUSEMENU_H

#include "input/Bindings.h"
#include "sim/Body.h"
#include "sim/World.h"

#include <array>
#include <string>
#include <vector>

struct GLFWwindow;

namespace ui::testing { class PauseMenuAccess; }

namespace ui {

enum class WindowMode { Borderless = 0, Windowed = 1 };

struct DisplaySettings { WindowMode windowMode = WindowMode::Borderless; bool vsync = true; int windowedX = 120, windowedY = 120, windowedWidth = 1920, windowedHeight = 1080; bool operator==(const DisplaySettings&) const = default; };
struct SimulationSettings { double minSimSpeed = 1.0 / 64.0, maxSimSpeed = 64.0, gravityStrength = sim::World::Params::kDefaultG, globalRestitution = sim::World::Params::kDefaultRestitution; bool gravityEnabled = true, collisionsEnabled = true; int velocityIterations = sim::World::Params::kDefaultVelocityIterations, positionIterations = sim::World::Params::kDefaultPositionIterations; bool operator==(const SimulationSettings&) const = default; };
struct CameraSettings { float lookSensitivity = 0.0025f, baseMoveSpeed = 40.0f, fovDegrees = 60.0f; bool invertY = false; bool operator==(const CameraSettings&) const = default; };
struct InterfaceSettings { int uiScaleIndex = 2, minimapZoomIndex = 1, pathLengthIndex = 3; bool showHud = true, showMinimap = true, showCoordinates = true, showCrosshair = true, drawPath = false, objectInfo = true, objectInfoMaterial = false, objectInfoVelocity = true, objectInfoMass = true, objectInfoRadius = true, objectInfoAngularSpeed = false, objectInfoBodyType = false; bool operator==(const InterfaceSettings&) const = default; };
struct SettingsBundle { DisplaySettings display{}; SimulationSettings simulation{}; CameraSettings camera{}; InterfaceSettings interface{}; bool operator==(const SettingsBundle&) const = default; };

enum class SettingsPage { Display = 0, Simulation = 1, Camera = 2, Interface = 3, Controls = 4 };
enum class RowKind { Header = 0, Toggle, Choice, Rebind };
enum class ActionPlacement { Top = 0, Bottom };

inline constexpr std::array<float, 7> kUiScaleChoices{{0.75f, 0.85f, 1.00f, 1.15f, 1.30f, 1.40f, 1.50f}};
inline constexpr std::array<float, 4> kMinimapZoomChoices{{20.0f, 40.0f, 80.0f, 160.0f}};
inline constexpr std::array<int, 5> kPathLengthChoices{{256, 512, 1024, 2048, 4096}};
inline constexpr std::array<double, 9> kMinSpeedChoices{{1.0 / 256.0, 1.0 / 128.0, 1.0 / 64.0, 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0}};
inline constexpr std::array<double, 9> kMaxSpeedChoices{{1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0}};
inline constexpr double kDefaultGravityStrength = sim::World::Params::kDefaultG;
inline constexpr std::array<double, 9> kGravityStrengthChoices{{kDefaultGravityStrength * 0.125, kDefaultGravityStrength * 0.25, kDefaultGravityStrength * 0.50, kDefaultGravityStrength * 0.75, kDefaultGravityStrength * 1.00, kDefaultGravityStrength * (4.0 / 3.0), kDefaultGravityStrength * 2.00, kDefaultGravityStrength * 4.00, kDefaultGravityStrength * 8.00}};
inline constexpr std::array<int, 8> kCollisionIterationChoices{{1, 2, 3, 4, 5, 6, 8, 10}};
inline constexpr std::array<double, 11> kGlobalRestitutionChoices{{0.00, 0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00}};
inline constexpr std::array<float, 9> kLookSensitivityChoices{{0.0008f, 0.0012f, 0.0016f, 0.0020f, 0.0025f, 0.0030f, 0.0038f, 0.0048f, 0.0060f}};
inline constexpr std::array<float, 7> kBaseMoveSpeedChoices{{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f}};
inline constexpr std::array<float, 8> kFovChoices{{50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f}};

struct ViewRow {
    std::string label;
    std::string value;
    RowKind kind = RowKind::Header;
    bool disabled = false;
    bool selected = false;
    bool boolValue = false;
    bool operator==(const ViewRow&) const = default;
};

struct ViewAction {
    int id = 0;
    ActionPlacement placement = ActionPlacement::Bottom;
    std::string label;
    bool selected = false;
    bool hovered = false;
    bool operator==(const ViewAction&) const = default;
};

struct ViewPopup {
    bool visible = false;
    std::string title;
    std::string body;
    std::string confirmLabel;
    std::string cancelLabel;
    bool confirmSelected = true;
    bool hoverConfirm = false;
    bool hoverCancel = false;
    bool operator==(const ViewPopup&) const = default;
};

struct MenuView {
    bool visible = false;
    std::string title;
    std::string subtitle;
    std::string sectionTitle;
    std::vector<std::string> tabs;
    int activeTabIndex = 0;
    int hoveredTabIndex = -1;
    int hoveredRowIndex = -1;
    int firstVisibleLineIndex = 0;
    bool keepSelectedVisible = true;
    std::string statusLine;
    bool statusWarning = false;
    std::string footerHint;
    std::vector<ViewRow> rows;
    std::vector<ViewAction> actions;
    ViewPopup popup{};
    bool operator==(const MenuView&) const = default;
};

enum class ActionKind { Apply = 0, ResetWorld, ResetControls, ResetSettings, Close, Exit };

class PauseMenu {
public:
    friend class testing::PauseMenuAccess;
    enum class PopupKind { None = 0, ResetSettings, ResetWorld, ResetControls, ExitToHome };

    [[nodiscard]] bool isOpen() const { return open_; }
    [[nodiscard]] float uiScale() const;
    [[nodiscard]] const DisplaySettings& displaySettings() const { return applied_.display; }
    [[nodiscard]] const SimulationSettings& simulationSettings() const { return applied_.simulation; }
    [[nodiscard]] const CameraSettings& cameraSettings() const { return applied_.camera; }
    [[nodiscard]] const InterfaceSettings& interfaceSettings() const { return applied_.interface; }

    void loadSettings(const std::string& path);
    void applyCurrentDisplaySettings(GLFWwindow* window);
    void updateEscapeState(GLFWwindow* window, bool& mouseCaptured, bool& firstMouse, double& lastFrameTime, double& accumulator, double& alpha, std::vector<sim::Body>& bodies);
    void handlePressedKey(GLFWwindow* window, int pressedKey, input::ControlBindings& controls, const std::string& controlsConfigPath);
    void handlePointerInput(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath, float scrollDeltaY = 0.0f);
    void updateContinuousInput(GLFWwindow* window, const input::ControlBindings& controls);
    [[nodiscard]] MenuView buildView(const input::ControlBindings& controls) const;
    bool consumeResetWorldRequest();

private:
    enum class FocusArea { Rows = 0, TopActions, BottomActions, Popup };

    bool open_ = false, escWasDown_ = false, resumeRequested_ = false, resetWorldRequested_ = false, awaitingRebind_ = false, popupConfirmSelected_ = true, leftMouseWasDown_ = false;
    int awaitingRebindAction_ = -1, selectedRow_ = 0, selectedAction_ = 0, firstVisibleLine_ = 0, lastClickedRow_ = -1, hoveredPageTab_ = -1, hoveredRow_ = -1, hoveredAction_ = -1;
    SettingsPage page_ = SettingsPage::Display, lastClickedPage_ = SettingsPage::Display;
    SettingsBundle applied_{}, draft_{};
    std::string settingsPath_, statusMessage_;
    PopupKind popup_ = PopupKind::None;
    FocusArea focusArea_ = FocusArea::Rows;
    std::array<bool, input::kMouseBindingMaxButtons> mouseButtonWasDown_{};
    double lastClickAt_ = -1.0, upNextRepeatAt_ = 0.0, downNextRepeatAt_ = 0.0, leftNextRepeatAt_ = 0.0, rightNextRepeatAt_ = 0.0;
    bool upHeld_ = false, downHeld_ = false, leftHeld_ = false, rightHeld_ = false, manualScroll_ = false, hoverPopupConfirm_ = false, hoverPopupCancel_ = false;

    void normalizeSettings();
    void saveSettings() const;
    static void applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings);
    [[nodiscard]] int rowCount() const;
    [[nodiscard]] bool pageDirty(SettingsPage page) const;
    [[nodiscard]] bool fieldDisabled(int row) const;
    [[nodiscard]] std::string disabledReason(int row) const;
    void cyclePage(int delta);
    void selectPage(SettingsPage page);
    void moveSelectionVertical(int delta, const input::ControlBindings& controls);
    void moveSelectionHorizontal(int delta);
    void activateFocused(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
    void applyCurrentPage(GLFWwindow* window);
    void resetAllSettings(GLFWwindow* window);
    void resetControlsToDefaults(input::ControlBindings& controls, const std::string& controlsConfigPath);
    bool beginRebindForSelectedRow();
    bool applyRebindCode(int code, input::ControlBindings& controls, const std::string& controlsConfigPath);
    void openPopup(PopupKind popup);
    void closePopup();
    void confirmPopup(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath);
    void appendCurrentPageRows(MenuView& view, const input::ControlBindings& controls) const;
    void adjustCurrentPageRow(int delta);
    [[nodiscard]] bool selectedRowActsAsToggle() const;
    void triggerAction(ActionKind action, GLFWwindow* window);
    bool triggerFocusedAction(GLFWwindow* window, const input::ControlBindings& controls, const std::string& controlsConfigPath);
};

} // namespace ui

#endif // PHYSICS3D_UI_PAUSEMENU_H

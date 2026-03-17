#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "input/Bindings.h"
#include "ui/PauseMenuController.h"

namespace ui::testing {
    class PauseMenuControllerAccess {
    public:
        static void openMenu(PauseMenuController& controller)
        {
            controller.open_ = true;
            controller.awaitingRebind_ = false;
            controller.awaitingRebindAction_ = -1;
            controller.ignoreNextRebindMousePress_ = false;
            controller.popupType_ = PauseMenuController::PopupType::None;
            controller.popupReturnTarget_ = PauseMenuController::FocusTarget::SettingsRow;
            controller.clearHoverState();
            controller.setSelectedSettingRow(-1);
            controller.focusTarget_ = PauseMenuController::FocusTarget::SettingsRow;
            controller.scrollLineOffset_ = 0;
            controller.statusMessage_.clear();
        }

        static void switchToPage(PauseMenuController& controller, const SettingsPage page)
        {
            controller.switchToPage(page);
        }

        static void switchPageByDelta(PauseMenuController& controller, const int delta)
        {
            controller.switchPage(delta);
        }

        static void setSelectedRow(PauseMenuController& controller, const int row)
        {
            controller.setSelectedSettingRow(row);
        }

        static void adjustSelectedSetting(
            PauseMenuController& controller,
            const int delta,
            const bool selectPopupDefaultAction = true)
        {
            controller.adjustSelectedSetting(delta, selectPopupDefaultAction);
        }

        static void activateFocusedControl(
            PauseMenuController& controller,
            input::ControlBindings& controls,
            const std::string& controlsConfigPath)
        {
            controller.activateFocusedControl(nullptr, controls, controlsConfigPath);
        }

        static void commitSelectedSetting(PauseMenuController& controller)
        {
            controller.commitSelectedSetting(nullptr);
        }

        static void resetAllSettings(PauseMenuController& controller)
        {
            controller.resetAllSettings(nullptr);
        }

        static bool beginRebindForRow(PauseMenuController& controller, const int row)
        {
            return controller.beginRebindForRow(row);
        }

        static bool applyRebindCode(
            PauseMenuController& controller,
            const int code,
            input::ControlBindings& controls,
            const std::string& controlsConfigPath)
        {
            return controller.applyRebindCode(code, controls, controlsConfigPath);
        }

        [[nodiscard]] static int currentPage(const PauseMenuController& controller)
        {
            return static_cast<int>(controller.settingsPage_);
        }

        [[nodiscard]] static int selectedRow(const PauseMenuController& controller)
        {
            return controller.selectedSettingRow_;
        }

        [[nodiscard]] static bool pendingSelectionEdit(const PauseMenuController& controller)
        {
            return controller.pendingSelectionEdit_;
        }

        [[nodiscard]] static bool popupOpen(const PauseMenuController& controller)
        {
            return controller.hasOpenPopup();
        }

        [[nodiscard]] static bool draftDrawPathEnabled(const PauseMenuController& controller)
        {
            return controller.draftSettings_.interface.drawPath;
        }

        [[nodiscard]] static bool awaitingRebind(const PauseMenuController& controller)
        {
            return controller.awaitingRebind_;
        }

        [[nodiscard]] static const std::string& statusMessage(const PauseMenuController& controller)
        {
            return controller.statusMessage_;
        }
    };
} // namespace ui::testing

namespace {
    void require(const bool condition, const std::string& message)
    {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void testPauseMenuPageSwitchClearsTransientState()
    {
        ui::PauseMenuController controller;
        ui::testing::PauseMenuControllerAccess::openMenu(controller);
        ui::testing::PauseMenuControllerAccess::switchToPage(controller, ui::SettingsPage::Interface);
        ui::testing::PauseMenuControllerAccess::setSelectedRow(controller, 4);
        ui::testing::PauseMenuControllerAccess::adjustSelectedSetting(controller, 1);

        require(ui::testing::PauseMenuControllerAccess::popupOpen(controller),
            "draw-path toggle should open a confirmation popup");
        require(ui::testing::PauseMenuControllerAccess::pendingSelectionEdit(controller),
            "draw-path toggle should preserve a pending draft while the popup is open");

        ui::testing::PauseMenuControllerAccess::switchPageByDelta(controller, 1);

        require(
            ui::testing::PauseMenuControllerAccess::currentPage(controller) == static_cast<int>(ui::SettingsPage::Controls),
            "page switching should advance to the next settings page");
        require(!ui::testing::PauseMenuControllerAccess::popupOpen(controller),
            "page switching should close any open popup");
        require(!ui::testing::PauseMenuControllerAccess::pendingSelectionEdit(controller),
            "page switching should discard pending edits");
        require(ui::testing::PauseMenuControllerAccess::selectedRow(controller) == -1,
            "page switching should clear the selected row");
    }

    void testPauseMenuDrawPathPopupConfirmationCreatesPendingApply()
    {
        ui::PauseMenuController controller;
        input::ControlBindings controls{};

        ui::testing::PauseMenuControllerAccess::openMenu(controller);
        ui::testing::PauseMenuControllerAccess::switchToPage(controller, ui::SettingsPage::Interface);
        ui::testing::PauseMenuControllerAccess::setSelectedRow(controller, 4);
        ui::testing::PauseMenuControllerAccess::adjustSelectedSetting(controller, 1);

        auto hud = controller.buildHud(controls, false);
        require(hud.showResetConfirm, "draw-path enable should present a confirmation popup");
        require(hud.resetConfirmTitleText == "WARNING",
            "draw-path confirmation should surface the warning title");
        require(hud.resetConfirmBodyText == "MAY IMPACT PERFORMANCE",
            "draw-path confirmation should display the performance warning");
        require(hud.resetConfirmSingleAcknowledge,
            "draw-path confirmation should use the single-acknowledge popup layout");
        require(hud.resetConfirmYesLabel == "I UNDERSTAND",
            "draw-path confirmation should expose the acknowledge label through the HUD");
        require(hud.selectedResetConfirmYes,
            "keyboard-opened draw-path confirmation should default to the acknowledge action");

        ui::testing::PauseMenuControllerAccess::activateFocusedControl(controller, controls, "");

        hud = controller.buildHud(controls, false);
        require(!hud.showResetConfirm, "confirming the popup should close it");
        require(
            hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Apply)].visible,
            "confirming draw-path enable should leave an apply action pending");
        require(ui::testing::PauseMenuControllerAccess::draftDrawPathEnabled(controller),
            "confirming the popup should update the draft interface settings");
        require(!controller.interfaceSettings().drawPath,
            "confirming the popup should not immediately mutate the applied settings");

        ui::testing::PauseMenuControllerAccess::commitSelectedSetting(controller);
        require(controller.interfaceSettings().drawPath,
            "committing the draft should apply the draw-path setting");
    }

    void testPauseMenuResetAllSettingsRestoresDefaults()
    {
        ui::PauseMenuController controller;
        ui::testing::PauseMenuControllerAccess::openMenu(controller);

        ui::testing::PauseMenuControllerAccess::switchToPage(controller, ui::SettingsPage::Simulation);
        ui::testing::PauseMenuControllerAccess::setSelectedRow(controller, 2);
        ui::testing::PauseMenuControllerAccess::adjustSelectedSetting(controller, 1);
        ui::testing::PauseMenuControllerAccess::commitSelectedSetting(controller);
        require(!controller.simulationSettings().gravityEnabled,
            "gravity toggle should be committed before reset");

        ui::testing::PauseMenuControllerAccess::switchToPage(controller, ui::SettingsPage::Interface);
        ui::testing::PauseMenuControllerAccess::setSelectedRow(controller, 1);
        ui::testing::PauseMenuControllerAccess::adjustSelectedSetting(controller, 1);
        ui::testing::PauseMenuControllerAccess::commitSelectedSetting(controller);
        require(!controller.interfaceSettings().showHud,
            "HUD toggle should be committed before reset");

        ui::testing::PauseMenuControllerAccess::resetAllSettings(controller);

        require(controller.simulationSettings() == ui::SimulationSettings{},
            "reset should restore default simulation settings");
        require(controller.interfaceSettings() == ui::InterfaceSettings{},
            "reset should restore default interface settings");
        require(controller.cameraSettings() == ui::CameraSettings{},
            "reset should restore default camera settings");
        require(controller.displaySettings() == ui::DisplaySettings{},
            "reset should restore default display settings");
    }

    void testPauseMenuSettingsPersistenceUsesDescriptorConfig()
    {
        ui::PauseMenuController controller;
        const std::filesystem::path configPath =
            std::filesystem::temp_directory_path() / "physics3d_pause_menu_settings_test.cfg";
        std::error_code ec;
        std::filesystem::remove(configPath, ec);

        {
            std::ofstream out(configPath, std::ios::trunc);
            require(static_cast<bool>(out), "settings persistence test should create a temp config");
            out << "WINDOW_MODE=WINDOWED\n";
            out << "WINDOWED_X=40\n";
            out << "WINDOWED_Y=60\n";
            out << "WINDOWED_WIDTH=1280\n";
            out << "WINDOWED_HEIGHT=720\n";
            out << "VSYNC=OFF\n";
            out << "MIN_SIM_SPEED=0.5\n";
            out << "MAX_SIM_SPEED=8\n";
            out << "GRAVITY_ENABLED=OFF\n";
            out << "GRAVITY_STRENGTH=" << (ui::pause_menu_shared::kDefaultGravityStrength * 2.0) << '\n';
            out << "COLLISIONS_ENABLED=OFF\n";
            out << "COLLISION_ITERATIONS=6\n";
            out << "POSITION_ITERATIONS=8\n";
            out << "GLOBAL_RESTITUTION=0.7\n";
            out << "LOOK_SENSITIVITY=0.0038\n";
            out << "BASE_MOVE_SPEED=50\n";
            out << "INVERT_Y=ON\n";
            out << "FOV_DEGREES=90\n";
            out << "UI_SCALE_INDEX=5\n";
            out << "SHOW_HUD=OFF\n";
            out << "SHOW_CROSSHAIR=OFF\n";
            out << "SHOW_PHYSICS_STATS=ON\n";
            out << "DRAW_PATH=ON\n";
            out << "OBJECT_INFO=OFF\n";
            out << "OBJECT_INFO_BODY_TYPE=ON\n";
        }

        controller.loadSettings(configPath.string());

        require(controller.displaySettings().windowMode == ui::WindowMode::Windowed,
            "loading settings should parse window mode through the descriptor persistence path");
        require(!controller.displaySettings().vsync,
            "loading settings should parse display toggles through the descriptor persistence path");
        require(controller.simulationSettings().velocityIterations == 6,
            "legacy velocity iteration keys should still load into the descriptor-backed field");
        require(controller.simulationSettings().positionIterations == 8,
            "position iteration values should round-trip through the descriptor-backed field");
        require(controller.cameraSettings().invertY,
            "camera settings should load through the descriptor-backed persistence path");
        require(controller.interfaceSettings().showPhysicsStats,
            "interface settings should honor legacy debug-stats config keys");

        std::ifstream in(configPath);
        require(static_cast<bool>(in), "settings persistence test should reopen the rewritten config");
        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string text = buffer.str();

        require(text.find("VELOCITY_ITERATIONS=6") != std::string::npos,
            "rewritten configs should use the descriptor-backed canonical velocity iterations key");
        require(text.find("COLLISION_ITERATIONS") == std::string::npos,
            "rewritten configs should drop the legacy collision iterations key");
        require(text.find("SHOW_DEBUG_STATS=ON") != std::string::npos,
            "rewritten configs should serialize descriptor-backed interface toggles");

        std::filesystem::remove(configPath, ec);
    }

    void testPauseMenuRebindingFlow()
    {
        ui::PauseMenuController controller;
        input::ControlBindings controls{};
        const std::filesystem::path configPath =
            std::filesystem::temp_directory_path() / "physics3d_pause_menu_test_controls.cfg";
        std::error_code ec;
        std::filesystem::remove(configPath, ec);

        ui::testing::PauseMenuControllerAccess::openMenu(controller);
        ui::testing::PauseMenuControllerAccess::switchToPage(controller, ui::SettingsPage::Controls);

        require(ui::testing::PauseMenuControllerAccess::beginRebindForRow(controller, 0),
            "controls page should allow entering rebind mode for a valid row");
        require(ui::testing::PauseMenuControllerAccess::awaitingRebind(controller),
            "beginning a rebind should put the controller into awaiting-rebind mode");

        require(
            ui::testing::PauseMenuControllerAccess::applyRebindCode(controller, GLFW_KEY_F2, controls, configPath.string()),
            "a unique bindable key should be accepted for rebinding");
        require(controls.freeze == GLFW_KEY_F2,
            "accepted rebinds should update the targeted control binding");
        require(!ui::testing::PauseMenuControllerAccess::awaitingRebind(controller),
            "successful rebinding should exit awaiting-rebind mode");

        require(ui::testing::PauseMenuControllerAccess::beginRebindForRow(controller, 1),
            "controller should allow a second rebind attempt");
        require(
            !ui::testing::PauseMenuControllerAccess::applyRebindCode(controller, GLFW_KEY_F2, controls, configPath.string()),
            "rebinding to an already-used input should be rejected");
        require(ui::testing::PauseMenuControllerAccess::awaitingRebind(controller),
            "failed rebinding should keep the controller in awaiting-rebind mode");
        require(ui::testing::PauseMenuControllerAccess::statusMessage(controller) == "INPUT ALREADY IN USE",
            "duplicate rebind attempts should surface the expected status message");

        std::filesystem::remove(configPath, ec);
    }
} // namespace

void appendPauseMenuControllerTests(std::vector<std::pair<std::string, std::function<void()>>>& tests)
{
    tests.emplace_back("pause_menu_page_switch_clears_transient_state", testPauseMenuPageSwitchClearsTransientState);
    tests.emplace_back("pause_menu_draw_path_popup_confirmation_creates_pending_apply", testPauseMenuDrawPathPopupConfirmationCreatesPendingApply);
    tests.emplace_back("pause_menu_reset_all_settings_restores_defaults", testPauseMenuResetAllSettingsRestoresDefaults);
    tests.emplace_back("pause_menu_settings_persistence_uses_descriptor_config", testPauseMenuSettingsPersistenceUsesDescriptorConfig);
    tests.emplace_back("pause_menu_rebinding_flow", testPauseMenuRebindingFlow);
}

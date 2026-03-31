#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestRegistry.h"

#include "ui/PauseMenu.h"
#include "ui/PauseMenuLayout.h"

namespace ui::testing {
class PauseMenuAccess {
public:
    static void openMenu(PauseMenu& menu) {
        menu.open_ = true;
        menu.draft_ = menu.applied_;
        menu.page_ = SettingsPage::Display;
        menu.focusArea_ = PauseMenu::FocusArea::Rows;
        menu.selectedRow_ = 0;
        menu.selectedAction_ = 0;
        menu.firstVisibleLine_ = 0;
        menu.popup_ = PauseMenu::PopupKind::None;
        menu.awaitingRebind_ = false;
        menu.awaitingRebindAction_ = -1;
        menu.manualScroll_ = false;
        menu.statusMessage_.clear();
    }

    static void selectPage(PauseMenu& menu, const SettingsPage page) {
        menu.selectPage(page);
    }

    static void setSelectedRow(PauseMenu& menu, const int row) {
        menu.selectedRow_ = row;
        menu.focusArea_ = PauseMenu::FocusArea::Rows;
    }

    static void moveHorizontal(PauseMenu& menu, const int delta, input::ControlBindings& controls) {
        (void)controls;
        menu.moveSelectionHorizontal(delta);
    }

    static const SettingsBundle& draft(const PauseMenu& menu) {
        return menu.draft_;
    }

    static int selectedRow(const PauseMenu& menu) {
        return menu.selectedRow_;
    }

    static int selectedAction(const PauseMenu& menu) {
        return menu.selectedAction_;
    }

    static int focusArea(const PauseMenu& menu) {
        return static_cast<int>(menu.focusArea_);
    }

    static bool selectedRowSupportsHorizontalRepeat(const PauseMenu& menu) {
        return menu.selectedRowSupportsHorizontalRepeat();
    }

    static void setStatus(PauseMenu& menu, const std::string& status, const bool warning) {
        menu.statusMessage_ = status;
        menu.awaitingRebind_ = warning;
    }

    static void showPopup(PauseMenu& menu, const PauseMenu::PopupKind popup) {
        menu.popup_ = popup;
        menu.popupConfirmSelected_ = true;
        menu.hoverPopupConfirm_ = true;
        menu.hoverPopupCancel_ = false;
    }
};
} // namespace ui::testing

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testUi2SpaceOnlyTogglesToggleRows()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    menu.handlePressedKey(nullptr, GLFW_KEY_SPACE, controls, "");
    require(!ui::testing::PauseMenuAccess::draft(menu).simulation.gravityEnabled,
        "space should toggle toggle rows");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);
    const double before = ui::testing::PauseMenuAccess::draft(menu).simulation.maxSimSpeed;
    menu.handlePressedKey(nullptr, GLFW_KEY_SPACE, controls, "");
    require(ui::testing::PauseMenuAccess::draft(menu).simulation.maxSimSpeed == before,
        "space should not adjust non-toggle rows");
}

void testUi2SimulationPageAppliesMultiRowEditsImmediately()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 4);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    require(!ui::testing::PauseMenuAccess::draft(menu).simulation.gravityEnabled,
        "edited rows should update the draft state");
    require(!ui::testing::PauseMenuAccess::draft(menu).simulation.collisionsEnabled,
        "additional edits should update the draft state");
    require(!menu.simulationSettings().gravityEnabled,
        "simulation settings should update immediately");
    require(!menu.simulationSettings().collisionsEnabled,
        "multiple simulation edits should apply immediately");
}

void testUi2SimulationPathEditsApplyImmediately()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 8);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.drawPath,
        "simulation page should update draw path immediately");
    require(menu.interfaceSettings().drawPath,
        "draw path should apply immediately from the simulation page");
}

void testUi2DisplayPageUsesImmediateModel()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Display);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);

    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(!ui::testing::PauseMenuAccess::draft(menu).display.vsync,
        "display settings should update the draft state");
    require(!menu.displaySettings().vsync,
        "display page changes should apply immediately");
}

void testUi2EnterActivatesSelectedSetting()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);

    menu.handlePressedKey(nullptr, GLFW_KEY_ENTER, controls, "");
    require(!menu.simulationSettings().gravityEnabled,
        "enter should activate the selected setting");
}

void testUi2ScrollKeepsSelectionAndLiveEdits()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    menu.handlePointerInput(nullptr, controls, "", -1.0f);
    require(ui::testing::PauseMenuAccess::selectedRow(menu) == 2,
        "scrolling should not clear the selected row");
    require(!ui::testing::PauseMenuAccess::draft(menu).simulation.gravityEnabled,
        "scrolling should not discard live edits");
    require(!menu.simulationSettings().gravityEnabled,
        "scrolling should not roll back immediate changes");
}

void testUi2LastRowDoesNotJumpToActionsEarly()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 8);

    menu.handlePressedKey(nullptr, GLFW_KEY_DOWN, controls, "");
    require(ui::testing::PauseMenuAccess::focusArea(menu) == 2,
        "down from the last enabled row should move to bottom actions");
    require(ui::testing::PauseMenuAccess::selectedAction(menu) == 0,
        "actions should start at the first action when leaving the list");
}

void testUi2BuildViewMapsMenuState()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setStatus(menu, "STATUS", true);
    ui::testing::PauseMenuAccess::showPopup(menu, ui::PauseMenu::PopupKind::ResetWorld);

    const auto view = menu.buildView(controls);
    require(view.visible, "view should preserve visibility");
    require(view.title == "PAUSED", "view should expose the menu title");
    require(view.activeTabIndex == static_cast<int>(ui::SettingsPage::Interface),
        "view should preserve active tab");
    require(view.rows.size() > 11 && view.rows[2].label == "SHOW SPEED" && view.rows[10].label == "DETAIL LEVEL" &&
            view.rows[11].label == "BACKDROP",
        "view should expose the decluttered interface layout");
    for (const auto& row : view.rows) {
        require(row.label != "INFO MATERIAL" && row.label != "INFO VELOCITY" && row.label != "INFO MASS" &&
                row.label != "INFO RADIUS" && row.label != "INFO ANGULAR" && row.label != "INFO TYPE",
            "interface view should not expose separate object info sub-options");
    }
    for (const auto& action : view.actions) {
        require(action.id != static_cast<int>(ui::ActionKind::ResetWorld) &&
                action.id != static_cast<int>(ui::ActionKind::ResetControls),
            "interface view should not expose bottom apply-style actions");
    }

    require(view.statusLine == "STATUS" && view.statusWarning,
        "view should preserve warning status messages");
    require(view.footerHint == "ESC CLOSE  TAB PAGE  ARROWS NAVIGATE/CHANGE  ENTER ACTIVATE",
        "view should expose the immediate-mode activation hint");
    require(view.popup.visible,
        "view should preserve popup state");
    require(view.popup.title == "ARE YOU SURE?",
        "view should preserve popup title");
}

void testUi2PathLengthDisablesWhenDrawPathOff()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 9);

    const auto before = ui::testing::PauseMenuAccess::draft(menu).interface.pathLengthIndex;
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.pathLengthIndex == before,
        "path length should not change while draw path is off");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 10);
    const auto colorBefore = ui::testing::PauseMenuAccess::draft(menu).interface.pathColorIndex;
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.pathColorIndex == colorBefore,
        "path color should not change while draw path is off");

    const auto view = menu.buildView(controls);
    require(view.rows.size() > 11 && view.rows[10].label == "PATH LENGTH" && view.rows[10].disabled,
        "path length row should be disabled when draw path is off");
    require(view.rows[11].label == "PATH COLOR" && view.rows[11].disabled,
        "path color row should be disabled when draw path is off");
}

void testUi2SpatialHudRowsStayIndependentOfInfoPanel()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 3);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    const auto minimapBefore = ui::testing::PauseMenuAccess::draft(menu).interface.showMinimap;
    const auto coordsBefore = ui::testing::PauseMenuAccess::draft(menu).interface.showCoordinates;
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 4);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 6);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    require(ui::testing::PauseMenuAccess::draft(menu).interface.showMinimap != minimapBefore,
        "minimap toggle should remain editable even with all info rows off");
    require(ui::testing::PauseMenuAccess::draft(menu).interface.showCoordinates != coordsBefore,
        "coordinates toggle should remain editable even with all info rows off");
}

void testUi2MinimapZoomDisablesWhenMinimapOff()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 4);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    const auto before = ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex;
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 5);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex == before,
        "minimap zoom should not change while minimap is off");

    const auto view = menu.buildView(controls);
    require(view.rows.size() > 6 && view.rows[6].label == "MINIMAP ZOOM" && view.rows[6].disabled,
        "minimap zoom row should be disabled when minimap is off");
}

void testUi2ObjectInfoDetailLevelUsesBasicAndFullOnly()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);

    auto view = menu.buildView(controls);
    require(view.rows.size() > 10 && view.rows[10].label == "DETAIL LEVEL" && view.rows[10].value == "BASIC",
        "detail level should default to BASIC");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 9);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.objectInfoMaterial,
        "FULL detail should enable material info");
    require(ui::testing::PauseMenuAccess::draft(menu).interface.objectInfoAngularSpeed,
        "FULL detail should enable angular info");
    require(ui::testing::PauseMenuAccess::draft(menu).interface.objectInfoBodyType,
        "FULL detail should enable type info");

    view = menu.buildView(controls);
    require(view.rows.size() > 10 && view.rows[10].value == "FULL",
        "detail level should expose FULL as the only expanded option");
}

void testUi2BackdropPresetAppliesImmediately()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 10);

    auto view = menu.buildView(controls);
    require(view.rows.size() > 11 && view.rows[11].label == "BACKDROP" && view.rows[11].value == "SUNNY",
        "backdrop row should default to SUNNY");

    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.backdropPresetIndex == 1,
        "backdrop edits should update the draft state");
    require(menu.interfaceSettings().backdropPresetIndex == 1,
        "backdrop edits should apply immediately");

    view = menu.buildView(controls);
    require(view.rows[11].value == "SPACE",
        "backdrop row should expose SPACE after stepping right");
}

void testUi2GravityStrengthUsesReciprocalAndIntegerLabels()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 3);

    auto view = menu.buildView(controls);
    require(view.rows.size() > 4 && view.rows[4].label == "GRAVITY STRENGTH" && view.rows[4].value == "1X",
        "gravity strength should default to 1X");

    ui::testing::PauseMenuAccess::moveHorizontal(menu, -9, controls);
    view = menu.buildView(controls);
    require(view.rows[4].value == "0.1X",
        "gravity strength should show decimal labels on the left side");

    ui::testing::PauseMenuAccess::moveHorizontal(menu, 18, controls);
    view = menu.buildView(controls);
    require(view.rows[4].value == "10X",
        "gravity strength should show integer labels on the right side");
}

void testUi2LayoutKeepsManualScrollOffset()
{
    ui::MenuView view{};
    view.visible = true;
    view.sectionTitle = "SETTINGS";
    view.firstVisibleLineIndex = 5;
    view.keepSelectedVisible = false;
    view.rows.push_back(ui::ViewRow{
        .label = "CONTROLS",
        .value = "",
        .kind = ui::RowKind::Header,
    });
    for (int i = 0; i < 24; ++i) {
        view.rows.push_back(ui::ViewRow{
            .label = "ACTION " + std::to_string(i),
            .value = "KEY",
            .kind = ui::RowKind::Rebind,
            .disabled = false,
            .selected = i == 0,
            .boolValue = false,
        });
    }

    const auto layout = ui::pause_menu_layout::buildPauseMenuLayout(1280.0f, 720.0f, 1.0f, view);
    require(layout.visibleLines < layout.totalLines,
        "test layout should overflow to exercise scrolling");
    require(layout.firstLine == 5,
        "layout should honor a manual first visible line");
}

void testUi2LayoutKeepsSelectedRowVisibleWhenMovingUp()
{
    ui::MenuView view{};
    view.visible = true;
    view.sectionTitle = "SETTINGS";
    view.firstVisibleLineIndex = 8;
    view.keepSelectedVisible = true;
    view.rows.push_back(ui::ViewRow{
        .label = "CONTROLS",
        .value = "",
        .kind = ui::RowKind::Header,
    });
    for (int i = 0; i < 24; ++i) {
        view.rows.push_back(ui::ViewRow{
            .label = "ACTION " + std::to_string(i),
            .value = "KEY",
            .kind = ui::RowKind::Rebind,
            .disabled = false,
            .selected = i == 1,
            .boolValue = false,
        });
    }

    const auto layout = ui::pause_menu_layout::buildPauseMenuLayout(1280.0f, 720.0f, 1.0f, view);
    const std::size_t selectedLine = 2;
    require(layout.visibleLines < layout.totalLines,
        "test layout should overflow to exercise selection pinning");
    require(selectedLine >= layout.firstLine,
        "layout should scroll upward enough to include the selected row");
    require(selectedLine < layout.firstLine + layout.visibleLines,
        "selected row should remain visible when keepSelectedVisible is enabled");
}

void testUi2LayoutUsesRemoteStyleTopButtons()
{
    ui::MenuView view{};
    view.visible = true;
    view.tabs = {"DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS"};
    view.actions = {
        {.id = static_cast<int>(ui::ActionKind::ResetSettings), .placement = ui::ActionPlacement::Top, .label = ""},
        {.id = static_cast<int>(ui::ActionKind::Close), .placement = ui::ActionPlacement::Top, .label = "X"},
        {.id = static_cast<int>(ui::ActionKind::Exit), .placement = ui::ActionPlacement::Top, .label = "EXIT TO HOME"},
    };

    const auto layout = ui::pause_menu_layout::buildPauseMenuLayout(1024.0f, 720.0f, 1.0f, view);
    require(!layout.tabRects.empty(),
        "layout should create tab rects");
    require(layout.actionRects.size() == view.actions.size(),
        "layout should create top action rects");
    require(layout.actionRect(0).x0 <= layout.tabRects[0].x0,
        "reset button should be pinned to the top-left side");
    require(layout.actionRect(0).y1 <= layout.tabRects[0].y0,
        "reset button should sit above the tabs");
    require(std::abs((layout.actionRect(0).x1 - layout.actionRect(0).x0) - (layout.actionRect(0).y1 - layout.actionRect(0).y0)) < 0.01f,
        "reset button should use a square icon-style layout");
    require(layout.actionRect(1).x1 >= layout.cardX1 - 18.0f,
        "close button should be pinned to the top-right side");
    require(layout.actionRect(1).y1 <= layout.tabRects[0].y0,
        "close button should sit above the tabs");
    require(layout.actionRect(2).y0 == layout.tabRects[0].y0,
        "exit button should share the tab row");
}

void testUi2NormalizedChoiceRanges()
{
    require(ui::kGravityStrengthChoices.front() == ui::kDefaultGravityStrength / 10.0,
        "gravity strength should support down to 1/10x");
    require(ui::kGravityStrengthChoices[8] == ui::kDefaultGravityStrength * 0.9,
        "gravity strength should step down through decimal values");
    require(ui::kGravityStrengthChoices[9] == ui::kDefaultGravityStrength,
        "gravity strength should retain 1x at the center");
    require(ui::kGravityStrengthChoices[10] == ui::kDefaultGravityStrength * 2.0,
        "gravity strength should step up to 2x immediately to the right of 1x");
    require(ui::kGravityStrengthChoices.back() == ui::kDefaultGravityStrength * 10.0,
        "gravity strength should now support up to 10x");
    require(ui::kCollisionIterationChoices[6] == 7 && ui::kCollisionIterationChoices[8] == 9,
        "collision iterations should use normalized single-step increments");
    require(ui::kLookSensitivityChoices[0] == 0.0010f &&
            ui::kLookSensitivityChoices[1] == 0.0015f &&
            ui::kLookSensitivityChoices[3] == 0.0025f &&
            ui::kLookSensitivityChoices.back() == 0.0060f,
        "look sensitivity should use a simple 0.0005-step range with enough headroom");
    require(ui::kBaseMoveSpeedChoices[6] == 70.0f,
        "camera move speed should include the normalized 70 step");
}

void testUi2ControlsPopupSupportsHorizontalNavigation()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Controls);
    ui::testing::PauseMenuAccess::showPopup(menu, ui::PauseMenu::PopupKind::ResetControls);

    menu.handlePressedKey(nullptr, GLFW_KEY_RIGHT, controls, "");
    auto view = menu.buildView(controls);
    require(view.popup.visible && !view.popup.confirmSelected,
        "right should move a controls-page popup selection to cancel");

    menu.handlePressedKey(nullptr, GLFW_KEY_LEFT, controls, "");
    view = menu.buildView(controls);
    require(view.popup.confirmSelected,
        "left should move a controls-page popup selection back to confirm");
}

void testUi2ControlsPageShowsFocusTargetBinding()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Controls);

    const auto view = menu.buildView(controls);
    bool foundFocusTarget = false;
    for (const auto& row : view.rows) {
        if (row.label != "FOCUS TARGET") {
            continue;
        }
        require(row.value == "F",
            "controls page should expose the default focus target binding");
        foundFocusTarget = true;
        break;
    }

    require(foundFocusTarget,
        "controls page should include a focus target action");
}

void testUi2LegacyControlsConfigPreservesExistingCustomBindings()
{
    const std::filesystem::path configPath =
        std::filesystem::temp_directory_path() / "physics3d_legacy_controls.cfg";
    std::error_code removeError;
    std::filesystem::remove(configPath, removeError);

    {
        std::ofstream out(configPath, std::ios::trunc);
        out << "FREEZE=SPACE\n";
        out << "SPEED_DOWN=MINUS\n";
        out << "SPEED_UP=EQUAL\n";
        out << "SPEED_RESET=1\n";
        out << "MOVE_FORWARD=F\n";
        out << "MOVE_BACK=S\n";
        out << "MOVE_LEFT=A\n";
        out << "MOVE_RIGHT=D\n";
        out << "MOVE_UP=E\n";
        out << "MOVE_DOWN=Q\n";
        out << "CAMERA_BOOST=LSHIFT\n";
    }

    input::ControlBindings controls{};
    input::loadControlBindings(controls, configPath.string());

    require(controls.moveForward == GLFW_KEY_F,
        "loading an older controls config should preserve explicit custom bindings");
    require(controls.focusTarget != GLFW_KEY_F && input::isBindableKey(controls.focusTarget),
        "new focus binding should yield to existing custom keys when migrating old configs");

    std::ifstream in(configPath);
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    require(contents.find("MOVE_FORWARD=F") != std::string::npos,
        "migrated controls config should keep the original custom binding");
    require(contents.find("FOCUS_TARGET=") != std::string::npos &&
            contents.find("FOCUS_TARGET=F") == std::string::npos,
        "migrated controls config should add focus target without stealing the custom key");

    std::filesystem::remove(configPath, removeError);
}

void testUi2HorizontalRepeatOnlyAppliesToDirectionalRows()
{
    ui::PauseMenu menu;
    ui::testing::PauseMenuAccess::openMenu(menu);

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Display);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 0);
    require(!ui::testing::PauseMenuAccess::selectedRowSupportsHorizontalRepeat(menu),
        "window mode should not repeat horizontally in immediate mode");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);
    require(!ui::testing::PauseMenuAccess::selectedRowSupportsHorizontalRepeat(menu),
        "toggle rows should not repeat horizontally in immediate mode");

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 3);
    require(ui::testing::PauseMenuAccess::selectedRowSupportsHorizontalRepeat(menu),
        "directional choice rows should still support held horizontal stepping");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    require(!ui::testing::PauseMenuAccess::selectedRowSupportsHorizontalRepeat(menu),
        "simulation toggles should not repeat horizontally");
}

} // namespace

void appendUiPauseMenuTests(test_registry::TestList& tests)
{
    tests.emplace_back("ui2_space_only_toggles_toggle_rows", testUi2SpaceOnlyTogglesToggleRows);
    tests.emplace_back("ui2_simulation_page_applies_multi_row_edits_immediately", testUi2SimulationPageAppliesMultiRowEditsImmediately);
    tests.emplace_back("ui2_simulation_path_edits_apply_immediately", testUi2SimulationPathEditsApplyImmediately);
    tests.emplace_back("ui2_display_page_uses_immediate_model", testUi2DisplayPageUsesImmediateModel);
    tests.emplace_back("ui2_enter_activates_selected_setting", testUi2EnterActivatesSelectedSetting);
    tests.emplace_back("ui2_scroll_keeps_selection_and_live_edits", testUi2ScrollKeepsSelectionAndLiveEdits);
    tests.emplace_back("ui2_last_row_does_not_jump_to_actions_early", testUi2LastRowDoesNotJumpToActionsEarly);
    tests.emplace_back("ui2_build_view_maps_menu_state", testUi2BuildViewMapsMenuState);
    tests.emplace_back("ui2_path_length_disables_when_draw_path_off", testUi2PathLengthDisablesWhenDrawPathOff);
    tests.emplace_back("ui2_spatial_hud_rows_stay_independent_of_info_panel", testUi2SpatialHudRowsStayIndependentOfInfoPanel);
    tests.emplace_back("ui2_minimap_zoom_disables_when_minimap_off", testUi2MinimapZoomDisablesWhenMinimapOff);
    tests.emplace_back("ui2_object_info_detail_level_uses_basic_and_full_only", testUi2ObjectInfoDetailLevelUsesBasicAndFullOnly);
    tests.emplace_back("ui2_backdrop_preset_applies_immediately", testUi2BackdropPresetAppliesImmediately);
    tests.emplace_back("ui2_gravity_strength_uses_reciprocal_and_integer_labels", testUi2GravityStrengthUsesReciprocalAndIntegerLabels);
    tests.emplace_back("ui2_layout_keeps_manual_scroll_offset", testUi2LayoutKeepsManualScrollOffset);
    tests.emplace_back("ui2_layout_keeps_selected_row_visible_when_moving_up", testUi2LayoutKeepsSelectedRowVisibleWhenMovingUp);
    tests.emplace_back("ui2_layout_uses_remote_style_top_buttons", testUi2LayoutUsesRemoteStyleTopButtons);
    tests.emplace_back("ui2_normalized_choice_ranges", testUi2NormalizedChoiceRanges);
    tests.emplace_back("ui2_controls_popup_supports_horizontal_navigation", testUi2ControlsPopupSupportsHorizontalNavigation);
    tests.emplace_back("ui2_controls_page_shows_focus_target_binding", testUi2ControlsPageShowsFocusTargetBinding);
    tests.emplace_back(
        "ui2_legacy_controls_config_preserves_existing_custom_bindings",
        testUi2LegacyControlsConfigPreservesExistingCustomBindings);
    tests.emplace_back("ui2_horizontal_repeat_only_applies_to_directional_rows", testUi2HorizontalRepeatOnlyAppliesToDirectionalRows);
}


#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

    static void applyCurrentPage(PauseMenu& menu) {
        menu.applyCurrentPage(nullptr);
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

    static void setStatus(PauseMenu& menu, const std::string& status, const bool warning) {
        menu.statusMessage_ = status;
        menu.awaitingRebind_ = warning;
    }

    static void focusBottomAction(PauseMenu& menu, const int actionIndex) {
        menu.focusArea_ = PauseMenu::FocusArea::BottomActions;
        menu.selectedAction_ = actionIndex;
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

bool hasApplyAction(const ui::MenuView& view) {
    for (const auto& action : view.actions) {
        if (action.id == static_cast<int>(ui::ActionKind::Apply)) {
            return true;
        }
    }
    return false;
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

void testUi2SimulationPageSupportsMultiRowDraft()
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
        "draft should retain the first edited row");
    require(!ui::testing::PauseMenuAccess::draft(menu).simulation.collisionsEnabled,
        "draft should retain additional edits on the same page");
    require(menu.simulationSettings().gravityEnabled,
        "applied settings should stay unchanged before apply");
    require(menu.simulationSettings().collisionsEnabled,
        "applied settings should stay unchanged before apply");
}

void testUi2InterfacePageIgnoresHiddenSimulationPathDraft()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 8);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.drawPath,
        "simulation page should be able to draft draw path changes");
    require(!menu.interfaceSettings().drawPath,
        "draw path should remain unapplied before simulation apply");

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    const auto cleanView = menu.buildView(controls);
    require(!hasApplyAction(cleanView),
        "interface page should not show apply for hidden simulation-owned path edits");

    ui::testing::PauseMenuAccess::setSelectedRow(menu, 4);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(!ui::testing::PauseMenuAccess::draft(menu).interface.showCoordinates,
        "interface page should still draft its own visible edits");

    ui::testing::PauseMenuAccess::applyCurrentPage(menu);
    require(!menu.interfaceSettings().drawPath,
        "applying interface page should not commit hidden simulation-owned path edits");
    require(!menu.interfaceSettings().showCoordinates,
        "applying interface page should commit visible interface edits");

    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Simulation);
    const auto simulationView = menu.buildView(controls);
    require(hasApplyAction(simulationView),
        "simulation page should still expose apply for pending path edits");
}

void testUi2DisplayPageUsesDraftApplyModel()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Display);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);

    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(!ui::testing::PauseMenuAccess::draft(menu).display.vsync,
        "display settings should edit draft state first");
    require(menu.displaySettings().vsync,
        "display settings should remain unapplied until apply");

    ui::testing::PauseMenuAccess::applyCurrentPage(menu);
    require(!menu.displaySettings().vsync,
        "display page changes should apply explicitly");
}

void testUi2ScrollKeepsSelectionAndDraft()
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
        "scrolling should not discard draft edits");
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
    ui::testing::PauseMenuAccess::focusBottomAction(menu, 0);
    ui::testing::PauseMenuAccess::setStatus(menu, "STATUS", true);
    ui::testing::PauseMenuAccess::showPopup(menu, ui::PauseMenu::PopupKind::ResetWorld);

    const auto view = menu.buildView(controls);
    require(view.visible, "view should preserve visibility");
    require(view.title == "PAUSED", "view should expose the menu title");
    require(view.activeTabIndex == static_cast<int>(ui::SettingsPage::Interface),
        "view should preserve active tab");
    require(view.rows.size() > 2 && view.rows[2].label == "SHOW HUD",
        "view should preserve row ordering");

    bool foundApply = false;
    for (const auto& action : view.actions) {
        if (action.id == static_cast<int>(ui::ActionKind::Apply)) {
            foundApply = true;
            require(action.placement == ui::ActionPlacement::Bottom,
                "apply should be exposed as a bottom action");
            require(action.selected, "focused bottom action should stay selected");
        }
    }
    require(foundApply, "view should expose the apply action when the page is dirty");

    require(view.statusLine == "STATUS" && view.statusWarning,
        "view should preserve warning status messages");
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

    const auto view = menu.buildView(controls);
    require(view.rows.size() > 10 && view.rows[10].label == "PATH LENGTH" && view.rows[10].disabled,
        "path length row should be disabled when draw path is off");
}

void testUi2SpatialHudRowsDisableWhenHudOff()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 1);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    const auto minimapBefore = ui::testing::PauseMenuAccess::draft(menu).interface.showMinimap;
    const auto coordsBefore = ui::testing::PauseMenuAccess::draft(menu).interface.showCoordinates;
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    const auto zoomBefore = ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex;
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 3);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 4);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    require(ui::testing::PauseMenuAccess::draft(menu).interface.showMinimap == minimapBefore,
        "minimap toggle should not change while show hud is off");
    require(ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex == zoomBefore,
        "minimap zoom should not change while show hud is off");
    require(ui::testing::PauseMenuAccess::draft(menu).interface.showCoordinates == coordsBefore,
        "coordinates toggle should not change while show hud is off");

    const auto view = menu.buildView(controls);
    require(view.rows.size() > 4 && view.rows[3].label == "SHOW MINIMAP" && view.rows[3].disabled,
        "show minimap should be disabled when show hud is off");
    require(view.rows.size() > 5 && view.rows[4].label == "MINIMAP ZOOM" && view.rows[4].disabled,
        "minimap zoom should be disabled when show hud is off");
    require(view.rows.size() > 5 && view.rows[5].label == "SHOW COORDS" && view.rows[5].disabled,
        "show coords should be disabled when show hud is off");
}

void testUi2MinimapZoomDisablesWhenMinimapOff()
{
    ui::PauseMenu menu;
    input::ControlBindings controls{};
    ui::testing::PauseMenuAccess::openMenu(menu);
    ui::testing::PauseMenuAccess::selectPage(menu, ui::SettingsPage::Interface);
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 2);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);

    const auto before = ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex;
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 3);
    ui::testing::PauseMenuAccess::moveHorizontal(menu, 1, controls);
    require(ui::testing::PauseMenuAccess::draft(menu).interface.minimapZoomIndex == before,
        "minimap zoom should not change while minimap is off");

    const auto view = menu.buildView(controls);
    require(view.rows.size() > 4 && view.rows[4].label == "MINIMAP ZOOM" && view.rows[4].disabled,
        "minimap zoom row should be disabled when minimap is off");
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
        {.id = static_cast<int>(ui::ActionKind::ResetSettings), .placement = ui::ActionPlacement::Top, .label = "R"},
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
    require(layout.actionRect(1).x1 >= layout.cardX1 - 18.0f,
        "close button should be pinned to the top-right side");
    require(layout.actionRect(1).y1 <= layout.tabRects[0].y0,
        "close button should sit above the tabs");
    require(layout.actionRect(2).y0 == layout.tabRects[0].y0,
        "exit button should share the tab row");
}

} // namespace

void appendUiPauseMenuTests(std::vector<std::pair<std::string, std::function<void()>>>& tests)
{
    tests.emplace_back("ui2_space_only_toggles_toggle_rows", testUi2SpaceOnlyTogglesToggleRows);
    tests.emplace_back("ui2_simulation_page_supports_multi_row_draft", testUi2SimulationPageSupportsMultiRowDraft);
    tests.emplace_back("ui2_interface_page_ignores_hidden_simulation_path_draft", testUi2InterfacePageIgnoresHiddenSimulationPathDraft);
    tests.emplace_back("ui2_display_page_uses_draft_apply_model", testUi2DisplayPageUsesDraftApplyModel);
    tests.emplace_back("ui2_scroll_keeps_selection_and_draft", testUi2ScrollKeepsSelectionAndDraft);
    tests.emplace_back("ui2_last_row_does_not_jump_to_actions_early", testUi2LastRowDoesNotJumpToActionsEarly);
    tests.emplace_back("ui2_build_view_maps_menu_state", testUi2BuildViewMapsMenuState);
    tests.emplace_back("ui2_path_length_disables_when_draw_path_off", testUi2PathLengthDisablesWhenDrawPathOff);
    tests.emplace_back("ui2_spatial_hud_rows_disable_when_hud_off", testUi2SpatialHudRowsDisableWhenHudOff);
    tests.emplace_back("ui2_minimap_zoom_disables_when_minimap_off", testUi2MinimapZoomDisablesWhenMinimapOff);
    tests.emplace_back("ui2_layout_keeps_manual_scroll_offset", testUi2LayoutKeepsManualScrollOffset);
    tests.emplace_back("ui2_layout_keeps_selected_row_visible_when_moving_up", testUi2LayoutKeepsSelectedRowVisibleWhenMovingUp);
    tests.emplace_back("ui2_layout_uses_remote_style_top_buttons", testUi2LayoutUsesRemoteStyleTopButtons);
}


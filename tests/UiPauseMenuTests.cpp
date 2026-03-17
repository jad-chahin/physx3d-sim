#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ui/PauseMenu.h"
#include "ui/OverlayRenderer.h"

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
        menu.popup_ = PauseMenu::PopupKind::None;
        menu.awaitingRebind_ = false;
        menu.awaitingRebindAction_ = -1;
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
        menu.moveSelectionHorizontal(delta, controls, "");
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
    ui::testing::PauseMenuAccess::setSelectedRow(menu, 6);

    menu.handlePressedKey(nullptr, GLFW_KEY_DOWN, controls, "");
    require(ui::testing::PauseMenuAccess::selectedRow(menu) == 7,
        "down should move onto the last row before leaving the list");
    require(ui::testing::PauseMenuAccess::focusArea(menu) == 0,
        "focus should remain on rows when landing on the last row");

    menu.handlePressedKey(nullptr, GLFW_KEY_DOWN, controls, "");
    require(ui::testing::PauseMenuAccess::focusArea(menu) == 2,
        "down from the last row should move to bottom actions");
    require(ui::testing::PauseMenuAccess::selectedAction(menu) == 0,
        "actions should start at the first action when leaving the list");
}

void testUi2OverlayAdapterMapsViewState()
{
    ui::MenuView view{};
    view.visible = true;
    view.activePage = ui::SettingsPage::Interface;
    view.statusLine = "STATUS";
    view.footerHint = "FOOTER";
    view.rows.push_back(ui::ViewRow{
        .label = "INTERFACE",
        .value = "",
        .hint = "",
        .kind = ui::RowKind::Header,
    });
    view.rows.push_back(ui::ViewRow{
        .label = "SHOW HUD",
        .value = "ON",
        .hint = "",
        .kind = ui::RowKind::Toggle,
        .selected = true,
        .edited = true,
        .boolValue = true,
    });
    view.actions.push_back(ui::ViewAction{
        .kind = ui::ActionKind::Apply,
        .label = "APPLY CHANGES",
        .visible = true,
        .selected = true,
    });
    view.popup = ui::ViewPopup{
        .visible = true,
        .title = "WARNING",
        .body = "MAY IMPACT PERFORMANCE",
        .confirmLabel = "I UNDERSTAND",
        .cancelLabel = "",
        .singleAction = true,
        .confirmSelected = true,
    };

    const auto hud = ui::toOverlayPauseMenuHud(view);
    require(hud.visible, "adapter should preserve visibility");
    require(hud.activePageIndex == static_cast<int>(ui::SettingsPage::Interface),
        "adapter should preserve active page");
    require(hud.selectedSettingLineIndex == 1,
        "adapter should map selected row index");
    require(hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Apply)].visible,
        "adapter should expose apply action visibility");
    require(hud.showResetConfirm && hud.resetConfirmSingleAcknowledge,
        "adapter should map single-action popup state");
    require(hud.resetConfirmTitleText == "WARNING",
        "adapter should preserve popup title");
}

} // namespace

void appendUiPauseMenuTests(std::vector<std::pair<std::string, std::function<void()>>>& tests)
{
    tests.emplace_back("ui2_space_only_toggles_toggle_rows", testUi2SpaceOnlyTogglesToggleRows);
    tests.emplace_back("ui2_simulation_page_supports_multi_row_draft", testUi2SimulationPageSupportsMultiRowDraft);
    tests.emplace_back("ui2_display_page_uses_draft_apply_model", testUi2DisplayPageUsesDraftApplyModel);
    tests.emplace_back("ui2_scroll_keeps_selection_and_draft", testUi2ScrollKeepsSelectionAndDraft);
    tests.emplace_back("ui2_last_row_does_not_jump_to_actions_early", testUi2LastRowDoesNotJumpToActionsEarly);
    tests.emplace_back("ui2_overlay_adapter_maps_view_state", testUi2OverlayAdapterMapsViewState);
}


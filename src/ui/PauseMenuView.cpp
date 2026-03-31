#include "ui/PauseMenuInternal.h"

namespace ui {
using namespace pause_menu_detail;

void PauseMenu::appendCurrentPageRows(MenuView& view, const input::ControlBindings& controls) const
{
    const auto appendRows = [&](const auto& descriptors, const auto& settings) {
        appendRowsFromDescriptors(
            view,
            descriptors,
            settings,
            selectedRow_,
            focusArea_ == FocusArea::Rows,
            [&](const int row) { return fieldDisabled(row); });
    };

    switch (page_) {
        case SettingsPage::Display:
            appendRows(kDisplayRows, draft_.display);
            return;
        case SettingsPage::Simulation:
            appendRows(kSimulationRows, draft_.simulation);
            view.rows.push_back(ViewRow{
                .label = "DRAW PATH",
                .value = formatToggle(draft_.interface.drawPath),
                .kind = RowKind::Toggle,
                .disabled = fieldDisabled(static_cast<int>(kSimulationRows.size())),
                .selected = focusArea_ == FocusArea::Rows && static_cast<int>(kSimulationRows.size()) == selectedRow_,
                .boolValue = draft_.interface.drawPath,
            });
            view.rows.push_back(ViewRow{
                .label = "PATH LENGTH",
                .value = std::to_string(kPathLengthChoices[static_cast<std::size_t>(draft_.interface.pathLengthIndex)]),
                .kind = RowKind::Choice,
                .disabled = fieldDisabled(static_cast<int>(kSimulationRows.size()) + 1),
                .selected = focusArea_ == FocusArea::Rows &&
                    static_cast<int>(kSimulationRows.size()) + 1 == selectedRow_,
                .boolValue = false,
            });
            view.rows.push_back(ViewRow{
                .label = "PATH COLOR",
                .value = kPathColorChoices[static_cast<std::size_t>(draft_.interface.pathColorIndex)],
                .kind = RowKind::Choice,
                .disabled = fieldDisabled(static_cast<int>(kSimulationRows.size()) + 2),
                .selected = focusArea_ == FocusArea::Rows &&
                    static_cast<int>(kSimulationRows.size()) + 2 == selectedRow_,
                .boolValue = false,
            });
            return;
        case SettingsPage::Camera:
            appendRows(kCameraRows, draft_.camera);
            return;
        case SettingsPage::Interface:
            appendRows(kInterfaceRows, draft_.interface);
            return;
        case SettingsPage::Controls:
            for (int i = 0; i < static_cast<int>(input::rebindActions().size()); ++i) {
                const auto& action = input::rebindActions()[static_cast<std::size_t>(i)];
                view.rows.push_back(ViewRow{
                    .label = action.label,
                    .value = input::keyNameForCode(controls.*(action.member)),
                    .kind = RowKind::Rebind,
                    .disabled = false,
                    .selected = focusArea_ == FocusArea::Rows && i == selectedRow_,
                    .boolValue = false,
                });
            }
            return;
    }
}

MenuView PauseMenu::buildView(const input::ControlBindings& controls) const
{
    MenuView view{};
    view.visible = open_;
    if (!open_) {
        return view;
    }

    view.title = "PAUSED";
    view.subtitle = "ESC: RESUME";
    view.sectionTitle = "SETTINGS";
    view.tabs.assign(kPageNames.begin(), kPageNames.end());
    view.activeTabIndex = static_cast<int>(page_);
    view.hoveredTabIndex = hoveredPageTab_;
    view.hoveredRowIndex = hoveredRow_;
    view.firstVisibleLineIndex = firstVisibleLine_;
    view.keepSelectedVisible = !manualScroll_;
    const int contextRow = hoveredRow_ >= 0 ? hoveredRow_ : selectedRow_;
    const std::string contextDisabledReason =
        fieldDisabled(contextRow) ? disabledReason(contextRow) : "";
    view.statusLine = isDisabledReasonMessage(statusMessage_) ? "" : statusMessage_;
    view.statusWarning = awaitingRebind_;
    view.footerHint = contextDisabledReason.empty()
        ? "ESC CLOSE  TAB PAGE  ARROWS NAVIGATE/CHANGE  ENTER ACTIVATE"
        : contextDisabledReason;
    view.rows.reserve(static_cast<std::size_t>(rowCount()) + 1);
    view.rows.push_back(ViewRow{
        .label = kPageNames[static_cast<std::size_t>(page_)],
        .value = "",
        .kind = RowKind::Header,
    });
    appendCurrentPageRows(view, controls);

    int topActionIndex = 0;
    int bottomActionIndex = 0;
    for (const auto& definition : kActionDefinitions) {
        if (!definition.visible(page_, controls)) {
            continue;
        }
        const bool isTopAction = definition.section == ActionSection::Top;
        const int sectionIndex = isTopAction ? topActionIndex++ : bottomActionIndex++;
        view.actions.push_back(ViewAction{
            .id = static_cast<int>(definition.kind),
            .placement = isTopAction ? ActionPlacement::Top : ActionPlacement::Bottom,
            .label = definition.label,
            .selected =
                (isTopAction && focusArea_ == FocusArea::TopActions && selectedAction_ == sectionIndex) ||
                (!isTopAction && focusArea_ == FocusArea::BottomActions && selectedAction_ == sectionIndex),
            .hovered = hoveredAction_ == static_cast<int>(definition.kind),
        });
    }

    if (popup_ != PopupKind::None) {
        view.popup.visible = true;
        view.popup.confirmSelected = popupConfirmSelected_;
        if (const auto* popupDefinition = popupDefinitionFor(popup_)) {
            view.popup.title = popupDefinition->title;
            view.popup.body = popupDefinition->body;
            view.popup.confirmLabel = popupDefinition->confirmLabel;
            view.popup.cancelLabel = popupDefinition->cancelLabel;
            view.popup.hoverConfirm = hoverPopupConfirm_;
            view.popup.hoverCancel = hoverPopupCancel_;
        }
    }

    return view;
}

} // namespace ui

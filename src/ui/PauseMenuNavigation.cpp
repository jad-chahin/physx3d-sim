#include "ui/PauseMenuInternal.h"

namespace ui {
using namespace pause_menu_detail;

int PauseMenu::rowCount() const
{
    switch (page_) {
        case SettingsPage::Display: return static_cast<int>(kDisplayRows.size());
        case SettingsPage::Simulation: return static_cast<int>(kSimulationRows.size()) + 3;
        case SettingsPage::Camera: return static_cast<int>(kCameraRows.size());
        case SettingsPage::Interface: return static_cast<int>(kInterfaceRows.size());
        case SettingsPage::Controls: return static_cast<int>(input::rebindActions().size());
    }
    return 0;
}

bool PauseMenu::fieldDisabled(const int row) const
{
    if (page_ == SettingsPage::Simulation) {
        return (row == static_cast<int>(kSimulationRows.size()) + 1 ||
                row == static_cast<int>(kSimulationRows.size()) + 2) &&
            !draft_.interface.drawPath;
    }
    if (page_ == SettingsPage::Interface) {
        if (row == kInterfaceRowMinimapZoom && !draft_.interface.showMinimap) {
            return true;
        }
        if (row == kInterfaceRowDetailLevel && !draft_.interface.objectInfo) {
            return true;
        }
    }
    return false;
}

std::string PauseMenu::disabledReason(const int row) const
{
    if (page_ == SettingsPage::Simulation) {
        if ((row == static_cast<int>(kSimulationRows.size()) + 1 ||
             row == static_cast<int>(kSimulationRows.size()) + 2) &&
            !draft_.interface.drawPath)
        {
            return "ENABLE DRAW PATH FIRST";
        }
        return "";
    }
    if (page_ == SettingsPage::Interface) {
        if (row == kInterfaceRowMinimapZoom && !draft_.interface.showMinimap) {
            return "ENABLE SHOW MINIMAP FIRST";
        }
        if (row == kInterfaceRowDetailLevel && !draft_.interface.objectInfo) {
            return "ENABLE OBJECT INFO FIRST";
        }
    }
    return "";
}

void PauseMenu::cyclePage(const int delta)
{
    const int next = (static_cast<int>(page_) + delta + 5) % 5;
    selectPage(static_cast<SettingsPage>(next));
}

void PauseMenu::selectPage(const SettingsPage page)
{
    page_ = page;
    focusArea_ = FocusArea::Rows;
    selectedRow_ = 0;
    selectedAction_ = 0;
    firstVisibleLine_ = 0;
    popup_ = PopupKind::None;
    popupConfirmSelected_ = true;
    awaitingRebind_ = false;
    awaitingRebindAction_ = -1;
    manualScroll_ = false;
    statusMessage_.clear();
}

void PauseMenu::moveSelectionVertical(const int delta, const input::ControlBindings& controls)
{
    if (popup_ != PopupKind::None) {
        return;
    }
    manualScroll_ = false;

    const auto visibleActions = [&](const ActionSection section) {
        return visibleActionsForSection(page_, section, controls);
    };
    const auto findEnabledRow = [&](const int start, const int step) {
        const int count = rowCount();
        for (int row = start; row >= 0 && row < count; row += step) {
            if (!fieldDisabled(row)) {
                return row;
            }
        }
        return -1;
    };

    if (focusArea_ == FocusArea::Rows) {
        const int count = rowCount();
        if (count <= 0) {
            const auto actions = visibleActions(ActionSection::Top);
            if (!actions.empty()) {
                focusArea_ = FocusArea::TopActions;
                selectedAction_ = 0;
            }
            return;
        }
        const int nextRow = findEnabledRow(selectedRow_ + delta, delta < 0 ? -1 : 1);
        if (nextRow < 0 && delta < 0) {
            const auto actions = visibleActions(ActionSection::Top);
            if (!actions.empty()) {
                focusArea_ = FocusArea::TopActions;
                selectedAction_ = static_cast<int>(actions.size()) - 1;
            }
            selectedRow_ = std::clamp(selectedRow_, 0, count - 1);
            return;
        }
        if (nextRow < 0) {
            const auto actions = visibleActions(ActionSection::Bottom);
            if (!actions.empty()) {
                focusArea_ = FocusArea::BottomActions;
                selectedAction_ = 0;
            }
            selectedRow_ = std::clamp(selectedRow_, 0, count - 1);
            return;
        }
        selectedRow_ = nextRow;
        return;
    }

    if (focusArea_ == FocusArea::TopActions) {
        const auto actions = visibleActions(ActionSection::Top);
        const int count = static_cast<int>(actions.size());
        if (count <= 0) {
            const int firstEnabledRow = findEnabledRow(0, 1);
            if (firstEnabledRow >= 0) {
                focusArea_ = FocusArea::Rows;
                selectedRow_ = firstEnabledRow;
            }
            return;
        }
        if (delta > 0) {
            const int nextAction = selectedAction_ + delta;
            if (nextAction >= count) {
                const int firstEnabledRow = findEnabledRow(0, 1);
                if (firstEnabledRow >= 0) {
                    focusArea_ = FocusArea::Rows;
                    selectedRow_ = firstEnabledRow;
                    selectedAction_ = 0;
                } else {
                    selectedAction_ = count - 1;
                }
                return;
            }
            selectedAction_ = std::clamp(nextAction, 0, count - 1);
            return;
        }
        selectedAction_ = std::clamp(selectedAction_ + delta, 0, count - 1);
        return;
    }

    if (focusArea_ == FocusArea::BottomActions) {
        const auto actions = visibleActions(ActionSection::Bottom);
        const int count = static_cast<int>(actions.size());
        if (count <= 0) {
            const int lastEnabledRow = findEnabledRow(rowCount() - 1, -1);
            if (lastEnabledRow >= 0) {
                focusArea_ = FocusArea::Rows;
                selectedRow_ = lastEnabledRow;
                selectedAction_ = 0;
            }
            return;
        }
        const int nextAction = selectedAction_ + delta;
        if (nextAction < 0) {
            const int lastEnabledRow = findEnabledRow(rowCount() - 1, -1);
            if (lastEnabledRow >= 0) {
                focusArea_ = FocusArea::Rows;
                selectedRow_ = lastEnabledRow;
                selectedAction_ = 0;
            } else {
                selectedAction_ = 0;
            }
            return;
        }
        selectedAction_ = std::clamp(nextAction, 0, count - 1);
    }
}

void PauseMenu::adjustCurrentPageRow(const int delta)
{
    if (popup_ != PopupKind::None) {
        popupConfirmSelected_ = delta <= 0;
        return;
    }

    if (focusArea_ != FocusArea::Rows || selectedRow_ < 0 || fieldDisabled(selectedRow_)) {
        return;
    }

    manualScroll_ = false;

    switch (page_) {
        case SettingsPage::Display:
            adjustRowFromDescriptors(kDisplayRows, draft_.display, static_cast<std::size_t>(selectedRow_), delta);
            break;
        case SettingsPage::Simulation:
            if (selectedRow_ < static_cast<int>(kSimulationRows.size())) {
                adjustRowFromDescriptors(
                    kSimulationRows,
                    draft_.simulation,
                    static_cast<std::size_t>(selectedRow_),
                    delta);
            } else if (selectedRow_ == static_cast<int>(kSimulationRows.size())) {
                draft_.interface.drawPath = !draft_.interface.drawPath;
            } else if (selectedRow_ == static_cast<int>(kSimulationRows.size()) + 1) {
                draft_.interface.pathLengthIndex =
                    std::clamp(draft_.interface.pathLengthIndex + delta, 0, static_cast<int>(kPathLengthChoices.size()) - 1);
            } else if (selectedRow_ == static_cast<int>(kSimulationRows.size()) + 2) {
                draft_.interface.pathColorIndex =
                    std::clamp(draft_.interface.pathColorIndex + delta, 0, static_cast<int>(kPathColorChoices.size()) - 1);
            }
            break;
        case SettingsPage::Camera:
            adjustRowFromDescriptors(kCameraRows, draft_.camera, static_cast<std::size_t>(selectedRow_), delta);
            break;
        case SettingsPage::Interface:
            adjustRowFromDescriptors(kInterfaceRows, draft_.interface, static_cast<std::size_t>(selectedRow_), delta);
            break;
        case SettingsPage::Controls:
            break;
    }
}

bool PauseMenu::selectedRowActsAsToggle() const
{
    if (focusArea_ != FocusArea::Rows || selectedRow_ < 0 || fieldDisabled(selectedRow_)) {
        return false;
    }

    switch (page_) {
        case SettingsPage::Display: return selectedRow_ == 1;
        case SettingsPage::Simulation:
            return selectedRow_ == 2 || selectedRow_ == 4 || selectedRow_ == static_cast<int>(kSimulationRows.size());
        case SettingsPage::Camera: return selectedRow_ == 2;
        case SettingsPage::Interface:
            return selectedRow_ != kInterfaceRowUiScale &&
                selectedRow_ != kInterfaceRowMinimapZoom &&
                selectedRow_ != kInterfaceRowDetailLevel &&
                selectedRow_ != kInterfaceRowBackdropPreset;
        case SettingsPage::Controls: return false;
    }
    return false;
}

bool PauseMenu::selectedRowSupportsHorizontalRepeat() const
{
    if (popup_ != PopupKind::None) {
        return false;
    }
    if (focusArea_ != FocusArea::Rows || selectedRow_ < 0 || fieldDisabled(selectedRow_)) {
        return false;
    }

    switch (page_) {
        case SettingsPage::Display:
            return false;
        case SettingsPage::Simulation:
            return selectedRow_ == 0 ||
                selectedRow_ == 1 ||
                selectedRow_ == 3 ||
                selectedRow_ == 5 ||
                selectedRow_ == 6 ||
                selectedRow_ == 7 ||
                selectedRow_ == static_cast<int>(kSimulationRows.size()) + 1 ||
                selectedRow_ == static_cast<int>(kSimulationRows.size()) + 2;
        case SettingsPage::Camera:
            return selectedRow_ == 0 ||
                selectedRow_ == 1 ||
                selectedRow_ == 3;
        case SettingsPage::Interface:
            return selectedRow_ == kInterfaceRowUiScale ||
                selectedRow_ == kInterfaceRowMinimapZoom ||
                selectedRow_ == kInterfaceRowDetailLevel ||
                selectedRow_ == kInterfaceRowBackdropPreset;
        case SettingsPage::Controls:
            return false;
    }
    return false;
}

void PauseMenu::moveSelectionHorizontal(const int delta, GLFWwindow* window)
{
    if (popup_ != PopupKind::None) {
        adjustCurrentPageRow(delta);
        return;
    }

    if (page_ == SettingsPage::Controls) {
        return;
    }

    const SettingsBundle before = draft_;
    adjustCurrentPageRow(delta);
    if (draft_ != before) {
        commitCurrentPageSettings(window);
    }
}

} // namespace ui

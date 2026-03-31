#include "ui/PauseMenuInternal.h"

namespace ui {
using namespace pause_menu_detail;

void PauseMenu::openPopup(const PopupKind popup)
{
    popup_ = popup;
    focusArea_ = FocusArea::Popup;
    popupConfirmSelected_ = true;
}

void PauseMenu::closePopup()
{
    popup_ = PopupKind::None;
    focusArea_ = FocusArea::Rows;
}

bool PauseMenu::beginRebindForSelectedRow()
{
    if (page_ != SettingsPage::Controls) {
        return false;
    }
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(input::rebindActions().size())) {
        return false;
    }
    awaitingRebind_ = true;
    awaitingRebindAction_ = selectedRow_;
    statusMessage_ = "PRESS A KEY OR MOUSE BUTTON";
    return true;
}

bool PauseMenu::applyRebindCode(
    const int code,
    input::ControlBindings& controls,
    const std::string& controlsConfigPath)
{
    if (!input::isBindableKey(code)) {
        statusMessage_ = "INPUT NOT BINDABLE";
        return false;
    }

    const auto& actions = input::rebindActions();
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (static_cast<int>(i) == awaitingRebindAction_) {
            continue;
        }
        if (controls.*(actions[i].member) == code) {
            statusMessage_ = "INPUT ALREADY IN USE";
            return false;
        }
    }

    controls.*(actions[static_cast<std::size_t>(awaitingRebindAction_)].member) = code;
    input::saveControlBindings(controls, controlsConfigPath);
    awaitingRebind_ = false;
    awaitingRebindAction_ = -1;
    statusMessage_.clear();
    return true;
}

void PauseMenu::resetControlsToDefaults(
    input::ControlBindings& controls,
    const std::string& controlsConfigPath)
{
    controls = input::ControlBindings{};
    input::saveControlBindings(controls, controlsConfigPath);
    statusMessage_.clear();
}

void PauseMenu::resetAllSettings(GLFWwindow* window)
{
    applied_ = SettingsBundle{};
    normalizeSettings();
    applyDisplaySettings(window, applied_.display);
    saveSettings();
    statusMessage_.clear();
}

void PauseMenu::confirmPopup(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath)
{
    switch (popup_) {
        case PopupKind::ResetSettings:
            resetAllSettings(window);
            closePopup();
            return;
        case PopupKind::ResetWorld:
            resetWorldRequested_ = true;
            statusMessage_.clear();
            closePopup();
            return;
        case PopupKind::ResetControls:
            resetControlsToDefaults(controls, controlsConfigPath);
            closePopup();
            return;
        case PopupKind::ExitToHome:
            if (window != nullptr) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            closePopup();
            return;
        case PopupKind::None:
            return;
    }
}

void PauseMenu::triggerAction(const ActionKind action)
{
    switch (action) {
        case ActionKind::ResetWorld: openPopup(PopupKind::ResetWorld); return;
        case ActionKind::ResetControls: openPopup(PopupKind::ResetControls); return;
        case ActionKind::ResetSettings: openPopup(PopupKind::ResetSettings); return;
        case ActionKind::Close: resumeRequested_ = true; return;
        case ActionKind::Exit: openPopup(PopupKind::ExitToHome); return;
    }
}

bool PauseMenu::triggerFocusedAction(
    const input::ControlBindings& controls,
    const std::string& controlsConfigPath)
{
    if (focusArea_ != FocusArea::TopActions && focusArea_ != FocusArea::BottomActions) {
        return false;
    }

    const auto actions = visibleActionsForSection(
        page_,
        focusArea_ == FocusArea::TopActions ? ActionSection::Top : ActionSection::Bottom,
        controls);
    if (selectedAction_ < 0 || selectedAction_ >= static_cast<int>(actions.size())) {
        return false;
    }

    (void)controls;
    (void)controlsConfigPath;
    triggerAction(actions[static_cast<std::size_t>(selectedAction_)]->kind);
    return true;
}

void PauseMenu::commitCurrentPageSettings(GLFWwindow* window)
{
    switch (page_) {
        case SettingsPage::Display:
            applied_.display = draft_.display;
            applyDisplaySettings(window, applied_.display);
            break;
        case SettingsPage::Simulation:
            applied_.simulation = draft_.simulation;
            applied_.interface.drawPath = draft_.interface.drawPath;
            applied_.interface.pathLengthIndex = draft_.interface.pathLengthIndex;
            applied_.interface.pathColorIndex = draft_.interface.pathColorIndex;
            break;
        case SettingsPage::Camera:
            applied_.camera = draft_.camera;
            break;
        case SettingsPage::Interface:
            applyInterfacePageSettings(applied_.interface, draft_.interface);
            break;
        case SettingsPage::Controls:
            return;
    }

    draft_ = applied_;
    saveSettings();
    statusMessage_.clear();
}

void PauseMenu::activateFocused(
    GLFWwindow* window,
    input::ControlBindings& controls,
    const std::string& controlsConfigPath)
{
    if (popup_ != PopupKind::None) {
        if (popupConfirmSelected_) {
            confirmPopup(window, controls, controlsConfigPath);
        } else {
            closePopup();
        }
        return;
    }

    if (triggerFocusedAction(controls, controlsConfigPath)) {
        return;
    }

    if (page_ == SettingsPage::Controls) {
        beginRebindForSelectedRow();
        return;
    }

    if (selectedRow_ >= 0 && !fieldDisabled(selectedRow_)) {
        moveSelectionHorizontal(1, window);
    }
}

bool PauseMenu::consumeResetWorldRequest()
{
    const bool requested = resetWorldRequested_;
    resetWorldRequested_ = false;
    return requested;
}

} // namespace ui

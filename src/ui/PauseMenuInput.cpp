#include "ui/PauseMenuInternal.h"
#include "ui/PauseMenuLayout.h"

namespace ui {
using namespace pause_menu_detail;

void PauseMenu::updateEscapeState(
    GLFWwindow* window,
    bool& mouseCaptured,
    bool& firstMouse,
    double& lastFrameTime,
    double& accumulator,
    double& alpha,
    std::vector<sim::Body>& bodies)
{
    if (window == nullptr) {
        return;
    }

    const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if ((escDown && !escWasDown_) || resumeRequested_) {
        if (awaitingRebind_) {
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            statusMessage_.clear();
            resumeRequested_ = false;
            escWasDown_ = escDown;
            return;
        }

        if (popup_ != PopupKind::None) {
            closePopup();
            resumeRequested_ = false;
            escWasDown_ = escDown;
            return;
        }

        open_ = resumeRequested_ ? false : !open_;
        resumeRequested_ = false;
        awaitingRebind_ = false;
        awaitingRebindAction_ = -1;
        statusMessage_.clear();
        hoveredPageTab_ = hoveredRow_ = hoveredAction_ = -1;
        hoverPopupConfirm_ = hoverPopupCancel_ = false;
        focusArea_ = FocusArea::Rows;
        selectedRow_ = 0;
        selectedAction_ = 0;
        firstVisibleLine_ = 0;
        popup_ = PopupKind::None;
        popupConfirmSelected_ = true;
        manualScroll_ = false;
        draft_ = applied_;

        if (open_) {
            mouseCaptured = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            mouseCaptured = true;
            firstMouse = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            lastFrameTime = glfwGetTime();
            accumulator = 0.0;
            alpha = 0.0;
            for (auto& body : bodies) {
                body.prevPosition = body.position;
                body.prevOrientation = body.orientation;
            }
        }
    }

    escWasDown_ = escDown;
}

void PauseMenu::handlePressedKey(
    GLFWwindow* window,
    const int pressedKey,
    input::ControlBindings& controls,
    const std::string& controlsConfigPath)
{
    if (!open_) {
        return;
    }

    if (awaitingRebind_) {
        if (pressedKey == GLFW_KEY_ESCAPE) {
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            statusMessage_.clear();
            return;
        }
        if (pressedKey != GLFW_KEY_UNKNOWN) {
            applyRebindCode(pressedKey, controls, controlsConfigPath);
        }
        return;
    }

    if (pressedKey >= GLFW_KEY_1 && pressedKey <= GLFW_KEY_5) {
        selectPage(pageFromNumberKey(pressedKey));
        return;
    }

    switch (pressedKey) {
        case GLFW_KEY_TAB:
            if (window != nullptr &&
                (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS))
            {
                cyclePage(-1);
            } else {
                cyclePage(1);
            }
            return;
        case GLFW_KEY_PAGE_UP:
            cyclePage(-1);
            return;
        case GLFW_KEY_PAGE_DOWN:
            cyclePage(1);
            return;
        case GLFW_KEY_UP:
            moveSelectionVertical(-1, controls);
            return;
        case GLFW_KEY_DOWN:
            moveSelectionVertical(1, controls);
            return;
        case GLFW_KEY_LEFT:
            moveSelectionHorizontal(-1, window);
            return;
        case GLFW_KEY_RIGHT:
            moveSelectionHorizontal(1, window);
            return;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            activateFocused(window, controls, controlsConfigPath);
            return;
        case GLFW_KEY_SPACE:
            if (selectedRowActsAsToggle()) {
                moveSelectionHorizontal(1, window);
            }
            return;
        default:
            return;
    }
}

void PauseMenu::handlePointerInput(
    GLFWwindow* window,
    input::ControlBindings& controls,
    const std::string& controlsConfigPath,
    const float scrollDeltaY)
{
    if (!open_) {
        return;
    }

    bool pressedMouseBinding = false;
    int pressedMouseBindingCode = GLFW_KEY_UNKNOWN;
    if (window != nullptr) {
        for (int button = 0; button < input::kMouseBindingMaxButtons; ++button) {
            const bool down = glfwGetMouseButton(window, button) == GLFW_PRESS;
            if (down && !mouseButtonWasDown_[static_cast<std::size_t>(button)] && !pressedMouseBinding) {
                pressedMouseBinding = true;
                pressedMouseBindingCode = input::bindingCodeFromMouseButton(button);
            }
            mouseButtonWasDown_[static_cast<std::size_t>(button)] = down;
        }
    }

    if (awaitingRebind_ && pressedMouseBinding) {
        applyRebindCode(pressedMouseBindingCode, controls, controlsConfigPath);
        return;
    }

    if (window == nullptr) {
        return;
    }

    hoveredPageTab_ = hoveredRow_ = hoveredAction_ = -1;
    hoverPopupConfirm_ = hoverPopupCancel_ = false;

    const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool clickPressed = leftMouseDown && !leftMouseWasDown_;
    leftMouseWasDown_ = leftMouseDown;

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    int ww = 0;
    int wh = 0;
    glfwGetWindowSize(window, &ww, &wh);
    if (fbw <= 0 || fbh <= 0 || ww <= 0 || wh <= 0) {
        return;
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    const auto xPx = static_cast<float>(cursorX * (static_cast<double>(fbw) / static_cast<double>(ww)));
    const auto yPx = static_cast<float>(cursorY * (static_cast<double>(fbh) / static_cast<double>(wh)));

    auto view = buildView(controls);
    auto layout = ui::pause_menu_layout::buildPauseMenuLayout(
        static_cast<float>(fbw),
        static_cast<float>(fbh),
        uiScale(),
        view);
    if (!manualScroll_) {
        firstVisibleLine_ = static_cast<int>(layout.firstLine);
    }

    if (popup_ == PopupKind::None &&
        std::fabs(scrollDeltaY) > 0.01f &&
        layout.visibleLines > 0 &&
        layout.totalLines > layout.visibleLines)
    {
        const int maxFirst = static_cast<int>(layout.totalLines - layout.visibleLines);
        const int lineDelta = scrollDeltaY > 0.0f
            ? -static_cast<int>(std::ceil(scrollDeltaY))
            : static_cast<int>(std::ceil(-scrollDeltaY));
        firstVisibleLine_ = std::clamp(firstVisibleLine_ + lineDelta, 0, maxFirst);
        manualScroll_ = true;
        view = buildView(controls);
        layout = ui::pause_menu_layout::buildPauseMenuLayout(
            static_cast<float>(fbw),
            static_cast<float>(fbh),
            uiScale(),
            view);
    }

    if (popup_ != PopupKind::None) {
        const auto popupButtons = ui::pause_menu_layout::buildPopupButtonsLayout(layout, view.popup);
        hoverPopupConfirm_ = popupButtons.yesButton.contains(xPx, yPx);
        hoverPopupCancel_ = popupButtons.noButton.contains(xPx, yPx);
        if (!clickPressed) {
            return;
        }
        if (popupButtons.yesButton.contains(xPx, yPx)) {
            popupConfirmSelected_ = true;
            confirmPopup(window, controls, controlsConfigPath);
        } else if (popupButtons.noButton.contains(xPx, yPx)) {
            popupConfirmSelected_ = false;
            closePopup();
        }
        return;
    }

    for (std::size_t i = 0; i < layout.tabRects.size(); ++i) {
        if (layout.tabRects[i].contains(xPx, yPx)) {
            hoveredPageTab_ = static_cast<int>(i);
            break;
        }
    }
    if (clickPressed && hoveredPageTab_ >= 0) {
        selectPage(static_cast<SettingsPage>(hoveredPageTab_));
        return;
    }

    for (std::size_t i = 0; i < view.actions.size(); ++i) {
        const auto& action = view.actions[i];
        if (!layout.actionRect(i).contains(xPx, yPx)) {
            continue;
        }

        hoveredAction_ = action.id;
        if (!clickPressed) {
            return;
        }

        triggerAction(actionKindFromId(action.id));
        return;
    }

    const int count = rowCount();
    if (count <= 0 || view.rows.size() <= 1) {
        return;
    }

    int clickedRow = -1;
    int clickedLine = -1;
    for (std::size_t i = 0; i < layout.visibleLines; ++i) {
        const std::size_t lineIdx = layout.firstLine + i;
        if (!layout.lineRect(i).contains(xPx, yPx)) {
            continue;
        }
        if (view.rows[lineIdx].kind == RowKind::Header) {
            continue;
        }
        clickedLine = static_cast<int>(lineIdx);
        clickedRow = clickedLine - 1;
        hoveredRow_ = clickedLine;
        break;
    }

    if (!clickPressed) {
        return;
    }

    if (clickedRow < 0 || clickedRow >= count) {
        return;
    }

    focusArea_ = FocusArea::Rows;
    selectedRow_ = clickedRow;
    manualScroll_ = false;

    const double clickAt = glfwGetTime();
    constexpr double kDoubleClickWindowSeconds = 0.35;
    const bool isDoubleClick =
        lastClickedPage_ == page_ &&
        lastClickedRow_ == clickedRow &&
        lastClickAt_ >= 0.0 &&
        (clickAt - lastClickAt_) <= kDoubleClickWindowSeconds;
    lastClickedPage_ = page_;
    lastClickedRow_ = clickedRow;
    lastClickAt_ = clickAt;

    if (fieldDisabled(clickedRow)) {
        if (isDisabledReasonMessage(statusMessage_)) {
            statusMessage_.clear();
        }
        return;
    }

    if (page_ == SettingsPage::Controls) {
        if (isDoubleClick) {
            beginRebindForSelectedRow();
        }
        return;
    }

    const auto widgets = layout.lineWidgets(
        static_cast<std::size_t>(clickedLine - static_cast<int>(layout.firstLine)),
        view.rows[static_cast<std::size_t>(clickedLine)]);

    switch (view.rows[static_cast<std::size_t>(clickedLine)].kind) {
        case RowKind::Toggle:
            if (widgets.checkbox.contains(xPx, yPx)) {
                moveSelectionHorizontal(1, window);
                return;
            }
            break;
        case RowKind::Choice:
            if (widgets.leftArrow.contains(xPx, yPx)) {
                moveSelectionHorizontal(-1, window);
                return;
            }
            if (widgets.rightArrow.contains(xPx, yPx)) {
                moveSelectionHorizontal(1, window);
                return;
            }
            break;
        case RowKind::Rebind:
        case RowKind::Header:
            break;
    }
}

void PauseMenu::updateContinuousInput(GLFWwindow* window, const input::ControlBindings& controls)
{
    if (!open_ || window == nullptr || awaitingRebind_) {
        return;
    }

    const double now = glfwGetTime();
    constexpr double kRepeatDelay = 0.35;
    constexpr double kRepeatInterval = 0.08;

    const auto handleRepeat = [&](const int key, bool& held, double& nextRepeatAt, const auto& action) {
        if (const bool down = glfwGetKey(window, key) == GLFW_PRESS; !down) {
            held = false;
            nextRepeatAt = 0.0;
            return;
        }

        if (!held) {
            held = true;
            nextRepeatAt = now + kRepeatDelay;
            return;
        }

        if (now >= nextRepeatAt) {
            nextRepeatAt = now + kRepeatInterval;
            action();
        }
    };

    handleRepeat(GLFW_KEY_UP, upHeld_, upNextRepeatAt_, [&] { moveSelectionVertical(-1, controls); });
    handleRepeat(GLFW_KEY_DOWN, downHeld_, downNextRepeatAt_, [&] { moveSelectionVertical(1, controls); });
    handleRepeat(GLFW_KEY_LEFT, leftHeld_, leftNextRepeatAt_, [&] {
        if (selectedRowSupportsHorizontalRepeat()) {
            moveSelectionHorizontal(-1, window);
        }
    });
    handleRepeat(GLFW_KEY_RIGHT, rightHeld_, rightNextRepeatAt_, [&] {
        if (selectedRowSupportsHorizontalRepeat()) {
            moveSelectionHorizontal(1, window);
        }
    });
}

} // namespace ui

//
// Created by Codex on 2026-03-07.
//

#include "ui/PauseMenuController.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace ui {
    namespace {
        struct ResolutionChoice {
            const char* label;
            int width;
            int height;
        };

        struct FullscreenModeChoice {
            int width = 1920;
            int height = 1080;
            int refreshRate = GLFW_DONT_CARE;
        };

        struct CachedDesktopMode {
            GLFWmonitor* monitor = nullptr;
            int width = 1920;
            int height = 1080;
            int refreshRate = GLFW_DONT_CARE;
        };

        constexpr std::array<ResolutionChoice, 7> kResolutionChoices{{
            {"MONITOR DEFAULT", 0, 0},
            {"1280X720", 1280, 720},
            {"1600X900", 1600, 900},
            {"1920X1080", 1920, 1080},
            {"2560X1440", 2560, 1440},
            {"3200X1800", 3200, 1800},
            {"3840X2160", 3840, 2160},
        }};

        constexpr std::array<float, 5> kUiScaleChoices{{
            0.85f,
            1.00f,
            1.15f,
            1.30f,
            1.45f,
        }};

        constexpr std::array<double, 9> kMinSpeedChoices{{
            1.0 / 256.0,
            1.0 / 128.0,
            1.0 / 64.0,
            1.0 / 32.0,
            1.0 / 16.0,
            1.0 / 8.0,
            1.0 / 4.0,
            1.0 / 2.0,
            1.0,
        }};

        constexpr std::array<double, 9> kMaxSpeedChoices{{
            1.0,
            2.0,
            4.0,
            8.0,
            16.0,
            32.0,
            64.0,
            128.0,
            256.0,
        }};

        constexpr std::array<float, 9> kLookSensitivityChoices{{
            0.0008f,
            0.0012f,
            0.0016f,
            0.0020f,
            0.0025f,
            0.0030f,
            0.0038f,
            0.0048f,
            0.0060f,
        }};

        constexpr std::array<float, 8> kBaseMoveSpeedChoices{{
            10.0f,
            20.0f,
            30.0f,
            40.0f,
            60.0f,
            80.0f,
            100.0f,
            140.0f,
        }};

        constexpr std::array<float, 8> kFovChoices{{
            50.0f,
            60.0f,
            70.0f,
            80.0f,
            90.0f,
            100.0f,
            110.0f,
            120.0f,
        }};

        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;
        constexpr int kFontH = 7;
        constexpr int kAdvance = 6;
        std::vector<CachedDesktopMode> g_cachedDesktopModes;

        int wrapIndex(const int idx, const int count) {
            if (count <= 0) {
                return 0;
            }
            const int mod = idx % count;
            return mod < 0 ? (mod + count) : mod;
        }

        template <typename T, std::size_t N>
        int closestChoiceIndex(const std::array<T, N>& choices, const T value) {
            int bestIdx = 0;
            double bestDiff = std::fabs(static_cast<double>(choices[0] - value));
            for (std::size_t i = 1; i < N; ++i) {
                const double diff = std::fabs(static_cast<double>(choices[i] - value));
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestIdx = static_cast<int>(i);
                }
            }
            return bestIdx;
        }

        std::string formatSpeedMultiplier(const double value) {
            char buffer[32];
            if (value >= 100.0) {
                std::snprintf(buffer, sizeof(buffer), "%.0fX", value);
            } else if (value >= 10.0) {
                std::snprintf(buffer, sizeof(buffer), "%.1fX", value);
            } else if (value >= 1.0) {
                std::snprintf(buffer, sizeof(buffer), "%.2fX", value);
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.4fX", value);
            }
            return buffer;
        }

        float measureMaxLinePx(const std::string& s, const float scalePx) {
            int lineChars = 0;
            int maxChars = 0;
            for (const char c : s) {
                if (c == '\n') {
                    maxChars = std::max(maxChars, lineChars);
                    lineChars = 0;
                    continue;
                }
                (void)c;
                lineChars += kAdvance;
            }
            maxChars = std::max(maxChars, lineChars);
            return static_cast<float>(maxChars) * scalePx;
        }

        float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
            const float unitWidth = measureMaxLinePx(s, 1.0f);
            if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) {
                return preferredScalePx;
            }
            return std::min(preferredScalePx, maxWidthPx / unitWidth);
        }

        GLFWmonitor* monitorForWindow(GLFWwindow* window) {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            if (monitors == nullptr || monitorCount <= 0) {
                return glfwGetPrimaryMonitor();
            }

            int wx = 0;
            int wy = 0;
            int ww = 0;
            int wh = 0;
            glfwGetWindowPos(window, &wx, &wy);
            glfwGetWindowSize(window, &ww, &wh);

            GLFWmonitor* bestMonitor = glfwGetPrimaryMonitor();
            int bestOverlap = -1;

            for (int i = 0; i < monitorCount; ++i) {
                GLFWmonitor* monitor = monitors[i];
                if (monitor == nullptr) {
                    continue;
                }

                int mx = 0;
                int my = 0;
                glfwGetMonitorPos(monitor, &mx, &my);
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode == nullptr) {
                    continue;
                }

                const int overlapW = std::max(0, std::min(wx + ww, mx + mode->width) - std::max(wx, mx));
                const int overlapH = std::max(0, std::min(wy + wh, my + mode->height) - std::max(wy, my));
                const int overlapArea = overlapW * overlapH;
                if (overlapArea > bestOverlap) {
                    bestOverlap = overlapArea;
                    bestMonitor = monitor;
                }
            }

            return bestMonitor;
        }

        void refreshDesktopModeCache() {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            if (monitors == nullptr || monitorCount <= 0) {
                return;
            }

            g_cachedDesktopModes.clear();
            g_cachedDesktopModes.reserve(static_cast<std::size_t>(monitorCount));
            for (int i = 0; i < monitorCount; ++i) {
                GLFWmonitor* monitor = monitors[i];
                if (monitor == nullptr) {
                    continue;
                }
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode == nullptr) {
                    continue;
                }
                g_cachedDesktopModes.push_back({
                    monitor,
                    mode->width,
                    mode->height,
                    mode->refreshRate > 0 ? mode->refreshRate : GLFW_DONT_CARE,
                });
            }
        }

        bool getCachedDesktopMode(const GLFWmonitor* monitor, FullscreenModeChoice& outChoice) {
            for (const auto& cached : g_cachedDesktopModes) {
                if (cached.monitor == monitor) {
                    outChoice.width = cached.width;
                    outChoice.height = cached.height;
                    outChoice.refreshRate = cached.refreshRate;
                    return true;
                }
            }
            return false;
        }

        FullscreenModeChoice chooseFullscreenMode(
            GLFWmonitor* monitor,
            const bool monitorDefault,
            const int requestedWidth,
            const int requestedHeight)
        {
            FullscreenModeChoice choice{};
            if (monitor == nullptr) {
                if (!monitorDefault) {
                    choice.width = requestedWidth;
                    choice.height = requestedHeight;
                }
                return choice;
            }

            if (const GLFWvidmode* current = glfwGetVideoMode(monitor); current != nullptr) {
                choice.width = current->width;
                choice.height = current->height;
                choice.refreshRate = current->refreshRate > 0 ? current->refreshRate : GLFW_DONT_CARE;
            }

            int modeCount = 0;
            const GLFWvidmode* modes = glfwGetVideoModes(monitor, &modeCount);
            if (modes == nullptr || modeCount <= 0) {
                if (!monitorDefault) {
                    choice.width = requestedWidth;
                    choice.height = requestedHeight;
                    choice.refreshRate = GLFW_DONT_CARE;
                }
                return choice;
            }

            const GLFWvidmode* best = nullptr;
            if (monitorDefault) {
                FullscreenModeChoice cachedDesktop{};
                if (getCachedDesktopMode(monitor, cachedDesktop)) {
                    for (int i = 0; i < modeCount; ++i) {
                        const GLFWvidmode& mode = modes[i];
                        if (mode.width != cachedDesktop.width || mode.height != cachedDesktop.height) {
                            continue;
                        }
                        if (best == nullptr || mode.refreshRate > best->refreshRate) {
                            best = &mode;
                        }
                    }
                }
                if (best == nullptr && modeCount > 0) {
                    best = &modes[0];
                }
            } else {
                for (int i = 0; i < modeCount; ++i) {
                    const GLFWvidmode& mode = modes[i];
                    if (mode.width != requestedWidth || mode.height != requestedHeight) {
                        continue;
                    }
                    if (best == nullptr || mode.refreshRate > best->refreshRate) {
                        best = &mode;
                    }
                }
            }

            if (best != nullptr) {
                choice.width = best->width;
                choice.height = best->height;
                choice.refreshRate = best->refreshRate > 0 ? best->refreshRate : GLFW_DONT_CARE;
            } else if (!monitorDefault) {
                choice.width = requestedWidth;
                choice.height = requestedHeight;
                choice.refreshRate = GLFW_DONT_CARE;
            }

            return choice;
        }
    } // namespace

    float PauseMenuController::uiScale() const {
        return uiScaleValue(appliedInterfaceSettings_.uiScaleIndex);
    }

    void PauseMenuController::applyCurrentDisplaySettings(GLFWwindow* window) {
        applyDisplaySettings(window, appliedDisplaySettings_);
        discardPendingEdit();
    }

    void PauseMenuController::updateEscapeState(
        GLFWwindow* window,
        bool& mouseCaptured,
        bool& firstMouse,
        double& lastTime,
        double& accumulator,
        std::vector<sim::Body>& bodies)
    {
        if (window == nullptr) {
            return;
        }

        const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escDown && !escWasDown_) {
            if (open_ && awaitingRebind_) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                statusMessage_.clear();
                escWasDown_ = escDown;
                return;
            }

            open_ = !open_;
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            discardPendingEdit();
            statusMessage_.clear();

            if (open_) {
                const int count = settingCount();
                if (count > 0) {
                    selectedSettingRow_ = std::clamp(selectedSettingRow_, 0, count - 1);
                } else {
                    selectedSettingRow_ = 0;
                }

                mouseCaptured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                mouseCaptured = true;
                firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                lastTime = glfwGetTime();
                accumulator = 0.0;
                for (auto& body : bodies) {
                    body.prevPosition = body.position;
                }
            }
        }
        escWasDown_ = escDown;
    }

    void PauseMenuController::handlePressedKey(
        GLFWwindow* window,
        const int pressedKey,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        if (!open_ || pressedKey == GLFW_KEY_UNKNOWN) {
            return;
        }
        if (!awaitingRebind_ && pressedKey == GLFW_KEY_ESCAPE) {
            return;
        }

        const auto& actions = input::rebindActions();

        if (awaitingRebind_) {
            if (pressedKey == GLFW_KEY_ESCAPE) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                statusMessage_.clear();
                return;
            }

            if (awaitingRebindAction_ < 0 || awaitingRebindAction_ >= controlCount()) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                statusMessage_ = "REBIND ERROR";
                return;
            }

            if (!input::isBindableKey(pressedKey)) {
                statusMessage_ = "KEY NOT BINDABLE";
                return;
            }

            for (int actionIndex = 0; actionIndex < controlCount(); ++actionIndex) {
                if (actionIndex == awaitingRebindAction_) {
                    continue;
                }
                if (controls.*(actions[actionIndex].member) == pressedKey) {
                    statusMessage_ = "KEY ALREADY IN USE";
                    return;
                }
            }

            controls.*(actions[awaitingRebindAction_].member) = pressedKey;
            input::saveControlBindings(controls, controlsConfigPath);

            statusMessage_.clear();
            markAppliedSelection();
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            return;
        }

        if (pressedKey == GLFW_KEY_TAB) {
            const bool shiftDown = window != nullptr &&
                ((glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                 (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS));
            switchPage(shiftDown ? -1 : 1);
            return;
        }

        if (pressedKey == GLFW_KEY_PAGE_UP) {
            switchPage(-1);
            return;
        }
        if (pressedKey == GLFW_KEY_PAGE_DOWN) {
            switchPage(1);
            return;
        }
        if (pressedKey == GLFW_KEY_1) {
            switchToPage(SettingsPage::Display);
            return;
        }
        if (pressedKey == GLFW_KEY_2) {
            switchToPage(SettingsPage::Simulation);
            return;
        }
        if (pressedKey == GLFW_KEY_3) {
            switchToPage(SettingsPage::Camera);
            return;
        }
        if (pressedKey == GLFW_KEY_4) {
            switchToPage(SettingsPage::Interface);
            return;
        }
        if (pressedKey == GLFW_KEY_5) {
            switchToPage(SettingsPage::Controls);
            return;
        }
        if (pressedKey == GLFW_KEY_X && window != nullptr) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        switch (pressedKey) {
            case GLFW_KEY_UP:
                selectPrev();
                return;
            case GLFW_KEY_DOWN:
                selectNext();
                return;
            case GLFW_KEY_LEFT:
                if (!isControlPage()) {
                    adjustSelectedSetting(-1);
                }
                return;
            case GLFW_KEY_RIGHT:
                if (!isControlPage()) {
                    adjustSelectedSetting(1);
                }
                return;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
                if (isControlPage()) {
                    const int actionCount = controlCount();
                    if (selectedSettingRow_ >= actionCount) {
                        controls = input::ControlBindings{};
                        input::saveControlBindings(controls, controlsConfigPath);
                        markAppliedSelection();
                        statusMessage_.clear();
                        return;
                    }
                    if (actionCount <= 0) {
                        statusMessage_ = "NO CONTROLS TO REBIND";
                        return;
                    }
                    awaitingRebindAction_ = std::clamp(selectedSettingRow_, 0, actionCount - 1);
                    awaitingRebind_ = true;
                    statusMessage_.clear();
                } else {
                    commitSelectedSetting(window);
                }
                return;
            default:
                return;
        }
    }

    void PauseMenuController::handlePointerInput(GLFWwindow* window, const input::ControlBindings& controls) {
        if (window == nullptr) {
            return;
        }

        const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clickPressed = leftMouseDown && !leftMouseWasDown_;
        leftMouseWasDown_ = leftMouseDown;

        if (!open_ || !clickPressed || awaitingRebind_) {
            return;
        }

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
        const float xPx = static_cast<float>(cursorX * (static_cast<double>(fbw) / static_cast<double>(ww)));
        const float yPx = static_cast<float>(cursorY * (static_cast<double>(fbh) / static_cast<double>(wh)));

        const float w = static_cast<float>(fbw);
        const float h = static_cast<float>(fbh);
        const float menuScale = std::clamp(uiScale(), 0.80f, 1.35f);
        const float baseScalePx = 2.0f * menuScale;
        const float cardW = std::min(w * 0.88f, 1360.0f);
        const float cardH = std::min(h * 0.92f, 1020.0f);
        const float cardX0 = (w - cardW) * 0.5f;
        const float cardY0 = (h - cardH) * 0.5f;
        const float cardY1 = cardY0 + cardH;
        const float cardX1 = cardX0 + cardW;
        const float infoWidth = cardW - 72.0f;

        const std::string helpLayoutReference =
            "UP DOWN: SELECT OPTION\nLEFT RIGHT: CHANGE\nENTER: APPLY";
        const float helpBaseY = cardY0 + 96.0f * menuScale;
        const float helpScalePx = fitScaleForWidth(helpLayoutReference, baseScalePx * 0.98f, infoWidth);
        const float helpLineStep = (static_cast<float>(kFontH) + 2.0f) * helpScalePx;
        const float helpReservedHeight = helpLineStep * 3.0f;
        const float statusSlotY = helpBaseY + helpReservedHeight + 10.0f * menuScale;
        const float statusReservedScalePx = baseScalePx * 0.95f;
        const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
        const float tabsSlotY = statusSlotY + statusReservedHeight + 12.0f * menuScale;

        constexpr std::array<SettingsPage, 5> kPageOrder{
            SettingsPage::Display,
            SettingsPage::Simulation,
            SettingsPage::Camera,
            SettingsPage::Interface,
            SettingsPage::Controls,
        };
        constexpr std::array<const char*, 5> kPageTabLabels{{"DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS"}};

        const float tabsScalePx = baseScalePx * 1.00f;
        const float tabPadX = 12.0f * menuScale;
        const float tabPadY = 7.0f * menuScale;
        const float tabGap = 12.0f * menuScale;
        std::array<float, 5> tabWidths{};
        for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
            tabWidths[i] = measureMaxLinePx(kPageTabLabels[i], tabsScalePx) + tabPadX * 2.0f;
        }

        const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * tabsScalePx + tabPadY * 2.0f;

        const float exitPadX = 12.0f * menuScale;
        const std::string exitLabel = "QUIT";
        const float exitWidth = measureMaxLinePx(exitLabel, tabsScalePx) + exitPadX * 2.0f;
        const float exitX1 = cardX1 - 18.0f * menuScale;
        const float exitX0 = exitX1 - exitWidth;
        const float exitY0 = tabsSlotY;
        const float exitY1 = exitY0 + tabHeight;
        if (xPx >= exitX0 && xPx <= exitX1 && yPx >= exitY0 && yPx <= exitY1) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        const float tabXStart = cardX0 + 18.0f * menuScale;
        float tabX = tabXStart;
        for (std::size_t i = 0; i < kPageOrder.size(); ++i) {
            const float x0 = tabX;
            const float x1 = x0 + tabWidths[i];
            const float y0 = tabsSlotY;
            const float y1 = tabsSlotY + tabHeight;
            if (xPx >= x0 && xPx <= x1 && yPx >= y0 && yPx <= y1) {
                switchToPage(kPageOrder[i]);
                return;
            }
            tabX = x1 + tabGap;
        }

        // Match pause-menu row layout in DebugOverlay::drawPauseMenu so row clicks select correctly.
        const DebugOverlay::PauseMenuHud hud = buildHud(controls, false);
        if (hud.lines.empty()) {
            return;
        }

        constexpr float kSettingsScalePx = 2.10f;
        float settingsY0 = std::max(cardY0 + 196.0f * menuScale, tabsSlotY + tabHeight + 16.0f * menuScale);
        const float sectionX0 = cardX0 + 26.0f * menuScale;
        const float sectionX1 = cardX1 - 26.0f * menuScale;
        const float settingsY1 = cardY1 - 20.0f * menuScale;
        if (settingsY1 - settingsY0 < 220.0f) {
            settingsY0 = settingsY1 - 220.0f;
        }

        float rowScalePx = kSettingsScalePx * menuScale * 1.8f;
        float maxRowWidth = 0.0f;
        for (const auto& line : hud.lines) {
            maxRowWidth = std::max(maxRowWidth, measureMaxLinePx(line.text, 1.0f));
        }
        const float rowAreaWidth = (sectionX1 - sectionX0) - 28.0f * menuScale;
        if (maxRowWidth > 0.0f && rowAreaWidth > 0.0f) {
            rowScalePx = std::min(rowScalePx, rowAreaWidth / maxRowWidth);
        }

        const std::string settingsTitle = "SETTINGS";
        const float sectionHeaderScalePx =
            fitScaleForWidth(settingsTitle, rowScalePx * 1.18f, (sectionX1 - sectionX0) - 28.0f);
        const float sectionHeaderY = settingsY0 + 10.0f * menuScale;
        const float linesStartY =
            sectionHeaderY + (static_cast<float>(kFontH) + 3.0f) * sectionHeaderScalePx + 9.0f * menuScale;
        const float rowStep = (static_cast<float>(kFontH) + 4.0f) * rowScalePx;
        const float maxLines = std::floor((settingsY1 - linesStartY - 10.0f * menuScale) / rowStep);

        const std::size_t totalLines = hud.lines.size();
        const std::size_t visibleLines = std::min<std::size_t>(
            totalLines,
            static_cast<std::size_t>(std::max(0.0f, maxLines)));

        std::size_t firstLine = 0;
        if (visibleLines > 0 && totalLines > visibleLines) {
            const int clampedSelected = std::clamp(hud.selectedSettingLineIndex, 0, static_cast<int>(totalLines - 1));
            const auto selected = static_cast<std::size_t>(clampedSelected);
            if (selected >= visibleLines) {
                firstLine = selected - visibleLines + 1;
            }
            const std::size_t maxFirst = totalLines - visibleLines;
            if (firstLine > maxFirst) {
                firstLine = maxFirst;
            }
        }

        const int count = settingCount();
        for (std::size_t i = 0; i < visibleLines; ++i) {
            const std::size_t lineIdx = firstLine + i;
            const float lineY = linesStartY + static_cast<float>(i) * rowStep;
            const float lineX0 = sectionX0 + 10.0f * menuScale;
            const float lineX1 = sectionX1 - 10.0f * menuScale;
            const float lineY0 = lineY - 2.0f * menuScale;
            const float lineY1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale;

            if (xPx < lineX0 || xPx > lineX1 || yPx < lineY0 || yPx > lineY1) {
                continue;
            }

            if (hud.lines[lineIdx].header) {
                return;
            }

            const int rowIndex = static_cast<int>(lineIdx) - 1;
            if (rowIndex < 0 || rowIndex >= count) {
                return;
            }

            if (selectedSettingRow_ != rowIndex) {
                discardPendingEdit();
                selectedSettingRow_ = rowIndex;
                statusMessage_.clear();
            }
            return;
        }
    }

    DebugOverlay::PauseMenuHud PauseMenuController::buildHud(
        const input::ControlBindings& controls,
        const bool advanceApplyIndicator) const
    {
        DebugOverlay::PauseMenuHud hud{};
        hud.visible = open_;
        if (!open_) {
            return hud;
        }

        hud.awaitingBind = awaitingRebind_;
        hud.selectedRowIsControl = isControlPage();

        if (awaitingRebind_ && awaitingRebindAction_ >= 0 && awaitingRebindAction_ < controlCount()) {
            hud.pendingAction = input::rebindActions()[awaitingRebindAction_].label;
        }

        hud.statusLine = statusMessage_;
        hud.activePageIndex = static_cast<int>(settingsPage_);

        int appliedRowOnCurrentPage = -1;
        if (applyIndicatorFrames_ > 0 && lastAppliedPage_ == settingsPage_) {
            appliedRowOnCurrentPage = lastAppliedRow_;
        }

        const DisplaySettings& displayRef =
            (settingsPage_ == SettingsPage::Display &&
             pendingSelectionEdit_ &&
             draftSettingsPage_ == settingsPage_)
            ? draftDisplaySettings_
            : appliedDisplaySettings_;
        const SimulationSettings& simRef =
            (settingsPage_ == SettingsPage::Simulation &&
             pendingSelectionEdit_ &&
             draftSettingsPage_ == settingsPage_)
            ? draftSimulationSettings_
            : appliedSimulationSettings_;
        const CameraSettings& cameraRef =
            (settingsPage_ == SettingsPage::Camera &&
             pendingSelectionEdit_ &&
             draftSettingsPage_ == settingsPage_)
            ? draftCameraSettings_
            : appliedCameraSettings_;
        const InterfaceSettings& interfaceRef =
            (settingsPage_ == SettingsPage::Interface &&
             pendingSelectionEdit_ &&
             draftSettingsPage_ == settingsPage_)
            ? draftInterfaceSettings_
            : appliedInterfaceSettings_;

        if (settingsPage_ == SettingsPage::Display) {
            hud.lines.push_back({"DISPLAY SETTINGS", true});
            hud.lines.push_back({"WINDOW MODE: " + formatMenuValue(windowModeLabel(displayRef.windowMode)), false});
            const bool resolutionLocked = displayRef.windowMode == WindowMode::Borderless;
            const std::string resolutionValue = resolutionLocked
                ? "BORDERLESS LOCKED"
                : resolutionLabel(displayRef.resolutionIndex);
            hud.lines.push_back({"RESOLUTION: " + formatMenuValue(resolutionValue), false, resolutionLocked});
            hud.lines.push_back({"VSYNC: " + formatMenuValue(displayRef.vsync ? "ON" : "OFF"), false});

            hud.selectedSettingLineIndex =
                1 + std::clamp(selectedSettingRow_, 0, displaySettingRowCount() - 1);
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, displaySettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Simulation) {
            hud.lines.push_back({"SIMULATION SETTINGS", true});
            hud.lines.push_back({"MIN SPEED: " + formatMenuValue(formatSpeedMultiplier(simRef.minSimSpeed)), false});
            hud.lines.push_back({"MAX SPEED: " + formatMenuValue(formatSpeedMultiplier(simRef.maxSimSpeed)), false});
            hud.lines.push_back(
                {"MAX STEPS PER FRAME: " + formatMenuValue(std::to_string(simRef.maxPhysicsStepsPerFrame)), false});

            hud.selectedSettingLineIndex =
                1 + std::clamp(selectedSettingRow_, 0, simulationSettingRowCount() - 1);
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, simulationSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            hud.lines.push_back({"CAMERA SETTINGS", true});
            char sensitivityBuffer[32];
            std::snprintf(sensitivityBuffer, sizeof(sensitivityBuffer), "%.4f", cameraRef.lookSensitivity);
            hud.lines.push_back({"LOOK SENSITIVITY: " + formatMenuValue(sensitivityBuffer), false});

            char moveSpeedBuffer[32];
            std::snprintf(moveSpeedBuffer, sizeof(moveSpeedBuffer), "%.0f", cameraRef.baseMoveSpeed);
            hud.lines.push_back({"BASE MOVE SPEED: " + formatMenuValue(moveSpeedBuffer), false});

            hud.lines.push_back({"INVERT Y: " + formatMenuValue(cameraRef.invertY ? "ON" : "OFF"), false});

            char fovBuffer[32];
            std::snprintf(fovBuffer, sizeof(fovBuffer), "%.0f DEG", cameraRef.fovDegrees);
            hud.lines.push_back({"FOV: " + formatMenuValue(fovBuffer), false});

            hud.selectedSettingLineIndex =
                1 + std::clamp(selectedSettingRow_, 0, cameraSettingRowCount() - 1);
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, cameraSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            hud.lines.push_back({"INTERFACE", true});

            char scaleBuffer[32];
            std::snprintf(scaleBuffer, sizeof(scaleBuffer), "%.2fX", uiScaleValue(interfaceRef.uiScaleIndex));
            hud.lines.push_back({"UI SCALE: " + formatMenuValue(scaleBuffer), false});
            hud.lines.push_back({"SHOW HUD: " + formatMenuValue(interfaceRef.showHud ? "ON" : "OFF"), false});
            hud.lines.push_back({"OBJECT INFO: " + formatMenuValue(interfaceRef.objectInfo ? "ON" : "OFF"), false});
            hud.lines.push_back({
                "OBJECT INFO MATERIAL: " + formatMenuValue(interfaceRef.objectInfoMaterial ? "ON" : "OFF"),
                false,
                !interfaceRef.objectInfo,
            });
            hud.lines.push_back({
                "OBJECT INFO VELOCITY: " + formatMenuValue(interfaceRef.objectInfoVelocity ? "ON" : "OFF"),
                false,
                !interfaceRef.objectInfo,
            });
            hud.lines.push_back({
                "OBJECT INFO MASS: " + formatMenuValue(interfaceRef.objectInfoMass ? "ON" : "OFF"),
                false,
                !interfaceRef.objectInfo,
            });
            hud.lines.push_back({
                "OBJECT INFO RADIUS: " + formatMenuValue(interfaceRef.objectInfoRadius ? "ON" : "OFF"),
                false,
                !interfaceRef.objectInfo,
            });

            hud.selectedSettingLineIndex =
                1 + std::clamp(selectedSettingRow_, 0, interfaceSettingRowCount() - 1);
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, interfaceSettingRowCount() - 1);
            }
        } else {
            hud.lines.push_back({"CONTROLS", true});
            const int actionCount = controlCount();
            for (int i = 0; i < actionCount; ++i) {
                const auto& action = input::rebindActions()[i];
                const int keyCode = controls.*(action.member);
                hud.lines.push_back({
                    std::string(action.label) + ": " + formatMenuValue(input::keyNameForCode(keyCode)),
                    false,
                });
            }
            hud.lines.push_back({"RESET CONTROLS", false});

            const int controlsPageRows = actionCount + 1;
            hud.selectedSettingLineIndex = 1 + std::clamp(selectedSettingRow_, 0, controlsPageRows - 1);
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, controlsPageRows - 1);
            }
        }

        if (advanceApplyIndicator && applyIndicatorFrames_ > 0) {
            --applyIndicatorFrames_;
            if (applyIndicatorFrames_ == 0) {
                lastAppliedRow_ = -1;
            }
        }

        return hud;
    }

    int PauseMenuController::settingCount() const {
        switch (settingsPage_) {
            case SettingsPage::Display:
                return displaySettingRowCount();
            case SettingsPage::Simulation:
                return simulationSettingRowCount();
            case SettingsPage::Camera:
                return cameraSettingRowCount();
            case SettingsPage::Interface:
                return interfaceSettingRowCount();
            case SettingsPage::Controls:
                return controlCount() + 1;
        }
        return 0;
    }

    int PauseMenuController::controlCount() {
        return static_cast<int>(input::rebindActions().size());
    }

    const char* PauseMenuController::resolutionLabel(int idx) {
        idx = std::clamp(idx, 0, resolutionChoiceCount() - 1);
        return kResolutionChoices[static_cast<std::size_t>(idx)].label;
    }

    int PauseMenuController::resolutionWidth(int idx) {
        idx = std::clamp(idx, 0, resolutionChoiceCount() - 1);
        return kResolutionChoices[static_cast<std::size_t>(idx)].width;
    }

    int PauseMenuController::resolutionHeight(int idx) {
        idx = std::clamp(idx, 0, resolutionChoiceCount() - 1);
        return kResolutionChoices[static_cast<std::size_t>(idx)].height;
    }

    int PauseMenuController::resolutionChoiceCount() {
        return static_cast<int>(kResolutionChoices.size());
    }

    int PauseMenuController::displaySettingRowCount() {
        return 3;
    }

    int PauseMenuController::simulationSettingRowCount() {
        return 3;
    }

    int PauseMenuController::cameraSettingRowCount() {
        return 4;
    }

    int PauseMenuController::interfaceSettingRowCount() {
        return 7;
    }

    float PauseMenuController::uiScaleValue(int idx) {
        idx = std::clamp(idx, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
        return kUiScaleChoices[static_cast<std::size_t>(idx)];
    }

    const char* PauseMenuController::windowModeLabel(const WindowMode mode) {
        switch (mode) {
            case WindowMode::Borderless:
                return "BORDERLESS";
            case WindowMode::Windowed:
                return "WINDOWED";
            case WindowMode::Fullscreen:
                return "FULLSCREEN";
        }
        return "UNKNOWN";
    }

    int PauseMenuController::wrapSelectionRow(const int row, const int count) {
        return wrapIndex(row, count);
    }

    const char* PauseMenuController::settingsPageLabel(const SettingsPage page) {
        switch (page) {
            case SettingsPage::Display:
                return "DISPLAY";
            case SettingsPage::Simulation:
                return "SIMULATION";
            case SettingsPage::Camera:
                return "CAMERA";
            case SettingsPage::Interface:
                return "INTERFACE";
            case SettingsPage::Controls:
                return "CONTROLS";
        }
        return "UNKNOWN";
    }

    std::string PauseMenuController::formatMenuValue(const char* value) {
        return std::string("= ") + value;
    }

    std::string PauseMenuController::formatMenuValue(const std::string& value) {
        return std::string("= ") + value;
    }

    bool PauseMenuController::isControlPage() const {
        return settingsPage_ == SettingsPage::Controls;
    }

    void PauseMenuController::discardPendingEdit() {
        draftDisplaySettings_ = appliedDisplaySettings_;
        draftSimulationSettings_ = appliedSimulationSettings_;
        draftCameraSettings_ = appliedCameraSettings_;
        draftInterfaceSettings_ = appliedInterfaceSettings_;
        draftSettingsPage_ = settingsPage_;
        draftSelectionRow_ = -1;
        pendingSelectionEdit_ = false;
    }

    void PauseMenuController::ensureDraftForSelectedRow() {
        if (pendingSelectionEdit_ &&
            (draftSettingsPage_ != settingsPage_ || draftSelectionRow_ != selectedSettingRow_))
        {
            discardPendingEdit();
        }

        if (!pendingSelectionEdit_) {
            draftDisplaySettings_ = appliedDisplaySettings_;
            draftSimulationSettings_ = appliedSimulationSettings_;
            draftCameraSettings_ = appliedCameraSettings_;
            draftInterfaceSettings_ = appliedInterfaceSettings_;
            draftSettingsPage_ = settingsPage_;
            draftSelectionRow_ = selectedSettingRow_;
            pendingSelectionEdit_ = true;
        }
    }

    void PauseMenuController::switchPage(const int delta) {
        switchToPage(static_cast<SettingsPage>(wrapIndex(static_cast<int>(settingsPage_) + delta, 5)));
    }

    void PauseMenuController::switchToPage(const SettingsPage page) {
        settingsPage_ = page;
        discardPendingEdit();
        awaitingRebind_ = false;
        awaitingRebindAction_ = -1;

        const int count = settingCount();
        if (count > 0) {
            selectedSettingRow_ = std::clamp(selectedSettingRow_, 0, count - 1);
        } else {
            selectedSettingRow_ = 0;
        }
        statusMessage_.clear();
    }

    void PauseMenuController::commitSelectedSetting(GLFWwindow* window) {
        if (isControlPage()) {
            return;
        }

        if (!pendingSelectionEdit_ ||
            draftSettingsPage_ != settingsPage_ ||
            draftSelectionRow_ != selectedSettingRow_)
        {
            statusMessage_.clear();
            return;
        }

        if (settingsPage_ == SettingsPage::Display) {
            appliedDisplaySettings_ = draftDisplaySettings_;
            applyDisplaySettings(window, appliedDisplaySettings_);
        } else if (settingsPage_ == SettingsPage::Simulation) {
            appliedSimulationSettings_ = draftSimulationSettings_;
        } else if (settingsPage_ == SettingsPage::Camera) {
            appliedCameraSettings_ = draftCameraSettings_;
        } else if (settingsPage_ == SettingsPage::Interface) {
            appliedInterfaceSettings_ = draftInterfaceSettings_;
        }
        markAppliedSelection();
        statusMessage_.clear();

        pendingSelectionEdit_ = false;
        draftSelectionRow_ = -1;
        draftSettingsPage_ = settingsPage_;
    }

    void PauseMenuController::selectNext() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        discardPendingEdit();
        selectedSettingRow_ = wrapSelectionRow(selectedSettingRow_ + 1, count);
        statusMessage_.clear();
    }

    void PauseMenuController::selectPrev() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        discardPendingEdit();
        selectedSettingRow_ = wrapSelectionRow(selectedSettingRow_ - 1, count);
        statusMessage_.clear();
    }

    void PauseMenuController::adjustSelectedSetting(const int delta) {
        if (isControlPage()) {
            return;
        }

        if (settingsPage_ == SettingsPage::Display && selectedSettingRow_ == 1) {
            const bool onSameDraftRow =
                pendingSelectionEdit_ &&
                draftSettingsPage_ == settingsPage_ &&
                draftSelectionRow_ == selectedSettingRow_;
            const WindowMode effectiveMode = onSameDraftRow
                ? draftDisplaySettings_.windowMode
                : appliedDisplaySettings_.windowMode;
            if (effectiveMode == WindowMode::Borderless) {
                statusMessage_ = "RESOLUTION LOCKED IN BORDERLESS";
                return;
            }
        }

        ensureDraftForSelectedRow();

        if (settingsPage_ == SettingsPage::Display) {
            auto& settings = draftDisplaySettings_;
            switch (selectedSettingRow_) {
                case 0:
                    settings.windowMode =
                        static_cast<WindowMode>(wrapSelectionRow(static_cast<int>(settings.windowMode) + delta, 3));
                    break;
                case 1:
                    settings.resolutionIndex =
                        wrapSelectionRow(settings.resolutionIndex + delta, resolutionChoiceCount());
                    break;
                case 2:
                    settings.vsync = !settings.vsync;
                    break;
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Simulation) {
            auto& settings = draftSimulationSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = wrapSelectionRow(
                        closestChoiceIndex(kMinSpeedChoices, settings.minSimSpeed) + delta,
                        static_cast<int>(kMinSpeedChoices.size()));
                    settings.minSimSpeed = kMinSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.maxSimSpeed < settings.minSimSpeed) {
                        settings.maxSimSpeed = settings.minSimSpeed;
                    }
                    break;
                }
                case 1: {
                    const int idx = wrapSelectionRow(
                        closestChoiceIndex(kMaxSpeedChoices, settings.maxSimSpeed) + delta,
                        static_cast<int>(kMaxSpeedChoices.size()));
                    settings.maxSimSpeed = kMaxSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.minSimSpeed > settings.maxSimSpeed) {
                        settings.minSimSpeed = settings.maxSimSpeed;
                    }
                    break;
                }
                case 2:
                    settings.maxPhysicsStepsPerFrame = std::clamp(settings.maxPhysicsStepsPerFrame + delta, 1, 20);
                    break;
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            auto& settings = draftCameraSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = wrapSelectionRow(
                        closestChoiceIndex(kLookSensitivityChoices, settings.lookSensitivity) + delta,
                        static_cast<int>(kLookSensitivityChoices.size()));
                    settings.lookSensitivity = kLookSensitivityChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 1: {
                    const int idx = wrapSelectionRow(
                        closestChoiceIndex(kBaseMoveSpeedChoices, settings.baseMoveSpeed) + delta,
                        static_cast<int>(kBaseMoveSpeedChoices.size()));
                    settings.baseMoveSpeed = kBaseMoveSpeedChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 2:
                    settings.invertY = !settings.invertY;
                    break;
                case 3: {
                    const int idx = wrapSelectionRow(
                        closestChoiceIndex(kFovChoices, settings.fovDegrees) + delta,
                        static_cast<int>(kFovChoices.size()));
                    settings.fovDegrees = kFovChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            auto& settings = draftInterfaceSettings_;
            switch (selectedSettingRow_) {
                case 0:
                    settings.uiScaleIndex = wrapSelectionRow(
                        settings.uiScaleIndex + delta,
                        static_cast<int>(kUiScaleChoices.size()));
                    break;
                case 1:
                    settings.showHud = !settings.showHud;
                    break;
                case 2:
                    settings.objectInfo = !settings.objectInfo;
                    break;
                case 3:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMaterial = !settings.objectInfoMaterial;
                    break;
                case 4:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoVelocity = !settings.objectInfoVelocity;
                    break;
                case 5:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMass = !settings.objectInfoMass;
                    break;
                case 6:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoRadius = !settings.objectInfoRadius;
                    break;
                default:
                    break;
            }
        }
        statusMessage_.clear();
    }

    void PauseMenuController::applyDisplaySettings(GLFWwindow* window, const DisplaySettings& settings) {
        if (window == nullptr) {
            return;
        }

        if (glfwGetWindowMonitor(window) == nullptr) {
            refreshDesktopModeCache();
        }

        if (glfwGetWindowMonitor(window) == nullptr &&
            glfwGetWindowAttrib(window, GLFW_DECORATED) == GLFW_TRUE)
        {
            glfwGetWindowPos(window, &windowedX_, &windowedY_);
            glfwGetWindowSize(window, &windowedWidth_, &windowedHeight_);
        }

        GLFWmonitor* monitor = monitorForWindow(window);
        const GLFWvidmode* monitorMode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        int monitorX = 0;
        int monitorY = 0;
        if (monitor != nullptr) {
            glfwGetMonitorPos(monitor, &monitorX, &monitorY);
        }

        int targetW = monitorMode != nullptr ? monitorMode->width : 1920;
        int targetH = monitorMode != nullptr ? monitorMode->height : 1080;
        if (settings.resolutionIndex > 0) {
            targetW = resolutionWidth(settings.resolutionIndex);
            targetH = resolutionHeight(settings.resolutionIndex);
        }

        switch (settings.windowMode) {
            case WindowMode::Borderless: {
                if (monitorMode != nullptr) {
                    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
                    glfwSetWindowMonitor(
                        window,
                        nullptr,
                        monitorX,
                        monitorY,
                        monitorMode->width,
                        monitorMode->height,
                        GLFW_DONT_CARE);
                }
                break;
            }
            case WindowMode::Windowed: {
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);

                int workX = monitorX;
                int workY = monitorY;
                int workW = monitorMode != nullptr ? monitorMode->width : targetW;
                int workH = monitorMode != nullptr ? monitorMode->height : targetH;
                if (monitor != nullptr) {
                    glfwGetMonitorWorkarea(monitor, &workX, &workY, &workW, &workH);
                }

                int frameL = 0;
                int frameT = 0;
                int frameR = 0;
                int frameB = 0;
                glfwGetWindowFrameSize(window, &frameL, &frameT, &frameR, &frameB);
                if (frameL < 0) frameL = 0;
                if (frameT < 0) frameT = 0;
                if (frameR < 0) frameR = 0;
                if (frameB < 0) frameB = 0;

                const int maxClientW = std::max(kMinWindowWidth, workW - frameL - frameR);
                const int maxClientH = std::max(kMinWindowHeight, workH - frameT - frameB);

                int useW = targetW;
                int useH = targetH;
                if (settings.resolutionIndex == 0) {
                    useW = maxClientW;
                    useH = maxClientH;
                }
                useW = std::clamp(useW, kMinWindowWidth, maxClientW);
                useH = std::clamp(useH, kMinWindowHeight, maxClientH);

                const int outerW = useW + frameL + frameR;
                const int outerH = useH + frameT + frameB;
                const int outerX = workX + (workW - outerW) / 2;
                const int outerY = workY + (workH - outerH) / 2;
                const int minPosX = workX + frameL;
                const int minPosY = workY + frameT;
                const int maxPosX = std::max(minPosX, workX + workW - frameR - useW);
                const int maxPosY = std::max(minPosY, workY + workH - frameB - useH);
                const int posX = std::clamp(outerX + frameL, minPosX, maxPosX);
                const int posY = std::clamp(outerY + frameT, minPosY, maxPosY);

                glfwSetWindowMonitor(window, nullptr, posX, posY, useW, useH, GLFW_DONT_CARE);
                windowedX_ = posX;
                windowedY_ = posY;
                windowedWidth_ = useW;
                windowedHeight_ = useH;
                break;
            }
            case WindowMode::Fullscreen: {
                if (monitor != nullptr) {
                    const FullscreenModeChoice modeChoice = chooseFullscreenMode(
                        monitor,
                        settings.resolutionIndex == 0,
                        targetW,
                        targetH);
                    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
                    glfwSetWindowMonitor(
                        window,
                        monitor,
                        0,
                        0,
                        modeChoice.width,
                        modeChoice.height,
                        modeChoice.refreshRate);
                }
                break;
            }
        }

        glfwSwapInterval(settings.vsync ? 1 : 0);
    }

    void PauseMenuController::markAppliedSelection() const {
        lastAppliedPage_ = settingsPage_;
        lastAppliedRow_ = selectedSettingRow_;
        applyIndicatorFrames_ = 120;
    }
} // namespace ui

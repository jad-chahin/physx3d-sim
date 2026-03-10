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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>

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
            {"MONITOR", 0, 0},
            {"1280X720", 1280, 720},
            {"1600X900", 1600, 900},
            {"1920X1080", 1920, 1080},
            {"2560X1440", 2560, 1440},
            {"3200X1800", 3200, 1800},
            {"3840X2160", 3840, 2160},
        }};

        constexpr std::array<float, 7> kUiScaleChoices{{
            0.80f,
            0.90f,
            1.00f,
            1.10f,
            1.20f,
            1.30f,
            1.40f,
        }};

        constexpr std::array<double, 11> kMinSpeedChoices{{
            1.0 / 1024.0,
            1.0 / 512.0,
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

        constexpr std::array<double, 11> kMaxSpeedChoices{{
            1.0,
            2.0,
            4.0,
            8.0,
            16.0,
            32.0,
            64.0,
            128.0,
            256.0,
            512.0,
            1024.0,
        }};

        constexpr double kDefaultGravityStrength = 6.6743e-11;
        constexpr std::array<double, 11> kGravityStrengthChoices{{
            kDefaultGravityStrength * 0.01,
            kDefaultGravityStrength * 0.05,
            kDefaultGravityStrength * 0.10,
            kDefaultGravityStrength * 0.25,
            kDefaultGravityStrength * 0.50,
            kDefaultGravityStrength * 1.00,
            kDefaultGravityStrength * 2.00,
            kDefaultGravityStrength * 5.00,
            kDefaultGravityStrength * 10.00,
            kDefaultGravityStrength * 25.00,
            kDefaultGravityStrength * 100.00,
        }};

        constexpr std::array<int, 8> kCollisionIterationChoices{{
            1, 2, 3, 4, 5, 6, 8, 10,
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

        std::string trimCopy(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        std::string toUpperCopy(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool tryParseBool(const std::string& value, bool& outValue) {
            const std::string needle = toUpperCopy(trimCopy(value));
            if (needle == "1" || needle == "TRUE" || needle == "ON" || needle == "YES") {
                outValue = true;
                return true;
            }
            if (needle == "0" || needle == "FALSE" || needle == "OFF" || needle == "NO") {
                outValue = false;
                return true;
            }
            return false;
        }

        bool tryParseInt(const std::string& value, int& outValue) {
            const std::string trimmed = trimCopy(value);
            if (trimmed.empty()) {
                return false;
            }
            try {
                std::size_t pos = 0;
                const int parsed = std::stoi(trimmed, &pos);
                if (pos != trimmed.size()) {
                    return false;
                }
                outValue = parsed;
                return true;
            } catch (...) {
                return false;
            }
        }

        bool tryParseDouble(const std::string& value, double& outValue) {
            const std::string trimmed = trimCopy(value);
            if (trimmed.empty()) {
                return false;
            }
            try {
                std::size_t pos = 0;
                const double parsed = std::stod(trimmed, &pos);
                if (pos != trimmed.size()) {
                    return false;
                }
                outValue = parsed;
                return true;
            } catch (...) {
                return false;
            }
        }

        bool tryParseFloat(const std::string& value, float& outValue) {
            double parsed = 0.0;
            if (!tryParseDouble(value, parsed)) {
                return false;
            }
            outValue = static_cast<float>(parsed);
            return true;
        }

        bool tryParseWindowMode(const std::string& value, WindowMode& outMode) {
            const std::string needle = toUpperCopy(trimCopy(value));
            if (needle == "BORDERLESS") {
                outMode = WindowMode::Borderless;
                return true;
            }
            if (needle == "WINDOWED") {
                outMode = WindowMode::Windowed;
                return true;
            }
            if (needle == "FULLSCREEN") {
                outMode = WindowMode::Fullscreen;
                return true;
            }
            return false;
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

        std::string formatGravityMultiplier(const double value) {
            char buffer[32];
            const double multiplier = value / kDefaultGravityStrength;
            if (multiplier >= 10.0) {
                std::snprintf(buffer, sizeof(buffer), "%.0fX", multiplier);
            } else if (multiplier >= 1.0) {
                std::snprintf(buffer, sizeof(buffer), "%.2fX", multiplier);
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.2fX", multiplier);
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

    void PauseMenuController::loadSettings(const std::string& path) {
        settingsConfigPath_ = path;

        std::ifstream in(path);
        if (in) {
            std::string line;
            while (std::getline(in, line)) {
                const auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }

                const std::string key = toUpperCopy(trimCopy(line.substr(0, eq)));
                const std::string value = trimCopy(line.substr(eq + 1));
                if (key.empty() || value.empty()) {
                    continue;
                }

                if (key == "WINDOW_MODE") {
                    WindowMode mode = WindowMode::Borderless;
                    if (tryParseWindowMode(value, mode)) {
                        appliedDisplaySettings_.windowMode = mode;
                    }
                } else if (key == "VSYNC") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedDisplaySettings_.vsync = parsed;
                    }
                } else if (key == "RESOLUTION_INDEX") {
                    int parsed = 0;
                    if (tryParseInt(value, parsed)) {
                        appliedDisplaySettings_.resolutionIndex = parsed;
                    }
                } else if (key == "MIN_SIM_SPEED") {
                    double parsed = 0.0;
                    if (tryParseDouble(value, parsed)) {
                        appliedSimulationSettings_.minSimSpeed = parsed;
                    }
                } else if (key == "MAX_SIM_SPEED") {
                    double parsed = 0.0;
                    if (tryParseDouble(value, parsed)) {
                        appliedSimulationSettings_.maxSimSpeed = parsed;
                    }
                } else if (key == "GRAVITY_ENABLED") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedSimulationSettings_.gravityEnabled = parsed;
                    }
                } else if (key == "GRAVITY_STRENGTH") {
                    double parsed = 0.0;
                    if (tryParseDouble(value, parsed)) {
                        appliedSimulationSettings_.gravityStrength = parsed;
                    }
                } else if (key == "COLLISION_ITERATIONS") {
                    int parsed = 0;
                    if (tryParseInt(value, parsed)) {
                        appliedSimulationSettings_.collisionIterations = parsed;
                    }
                } else if (key == "LOOK_SENSITIVITY") {
                    float parsed = 0.0f;
                    if (tryParseFloat(value, parsed)) {
                        appliedCameraSettings_.lookSensitivity = parsed;
                    }
                } else if (key == "BASE_MOVE_SPEED") {
                    float parsed = 0.0f;
                    if (tryParseFloat(value, parsed)) {
                        appliedCameraSettings_.baseMoveSpeed = parsed;
                    }
                } else if (key == "INVERT_Y") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedCameraSettings_.invertY = parsed;
                    }
                } else if (key == "FOV_DEGREES") {
                    float parsed = 0.0f;
                    if (tryParseFloat(value, parsed)) {
                        appliedCameraSettings_.fovDegrees = parsed;
                    }
                } else if (key == "UI_SCALE_INDEX") {
                    int parsed = 0;
                    if (tryParseInt(value, parsed)) {
                        appliedInterfaceSettings_.uiScaleIndex = parsed;
                    }
                } else if (key == "SHOW_HUD") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.showHud = parsed;
                    }
                } else if (key == "SHOW_CROSSHAIR") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.showCrosshair = parsed;
                    }
                } else if (key == "OBJECT_INFO") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfo = parsed;
                    }
                } else if (key == "OBJECT_INFO_MATERIAL") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoMaterial = parsed;
                    }
                } else if (key == "OBJECT_INFO_VELOCITY") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoVelocity = parsed;
                    }
                } else if (key == "OBJECT_INFO_MASS") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoMass = parsed;
                    }
                } else if (key == "OBJECT_INFO_RADIUS") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoRadius = parsed;
                    }
                }
            }
        }

        normalizeAppliedSettings();
        discardPendingEdit();
        saveSettings();
    }

    void PauseMenuController::applyCurrentDisplaySettings(GLFWwindow* window) {
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedDisplaySettings_);
        discardPendingEdit();
        saveSettings();
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
        const bool backDown = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        const bool closePressed = (escDown && !escWasDown_) || (backDown && !backWasDown_) || resumeRequested_;
        if (closePressed) {
            if (open_ && awaitingRebind_) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_.clear();
                resumeRequested_ = false;
                escWasDown_ = escDown;
                backWasDown_ = backDown;
                return;
            }

            const bool triggeredByResume = resumeRequested_;
            open_ = !open_;
            if (triggeredByResume) {
                open_ = false;
            }
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            ignoreNextRebindMousePress_ = false;
            resumeRequested_ = false;
            hoveredSettingRow_ = -1;
            hoveredDisplayApplyAction_ = false;
            hoveredResetControlsAction_ = false;
            hoveredResetIcon_ = false;
            hoveredResetConfirmYes_ = false;
            hoveredResetConfirmNo_ = false;
            confirmingResetSettings_ = false;
            discardPendingEdit();
            statusMessage_.clear();
            upHeld_ = false;
            downHeld_ = false;
            leftHeld_ = false;
            rightHeld_ = false;

            if (open_) {
                const int count = settingCount();
                if (count > 0 && selectedSettingRow_ >= 0) {
                    setSelectedSettingRow(std::clamp(selectedSettingRow_, 0, count - 1));
                } else {
                    setSelectedSettingRow(-1);
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
        backWasDown_ = backDown;
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
        if (!awaitingRebind_ &&
            !confirmingResetSettings_ &&
            (pressedKey == GLFW_KEY_ESCAPE || pressedKey == GLFW_KEY_BACKSPACE))
        {
            return;
        }

        if (awaitingRebind_) {
            if (pressedKey == GLFW_KEY_ESCAPE || pressedKey == GLFW_KEY_BACKSPACE) {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_.clear();
                return;
            }
            ignoreNextRebindMousePress_ = false;
            applyRebindCode(pressedKey, controls, controlsConfigPath);
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

        if (confirmingResetSettings_) {
            if (pressedKey == GLFW_KEY_ENTER || pressedKey == GLFW_KEY_KP_ENTER || pressedKey == GLFW_KEY_SPACE) {
                resetAllSettings(window);
                confirmingResetSettings_ = false;
            } else if (pressedKey == GLFW_KEY_ESCAPE || pressedKey == GLFW_KEY_BACKSPACE) {
                confirmingResetSettings_ = false;
            }
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
                    adjustSelectedSetting(-1, window);
                }
                return;
            case GLFW_KEY_RIGHT:
                if (!isControlPage()) {
                    adjustSelectedSetting(1, window);
                }
                return;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
            case GLFW_KEY_SPACE:
                if (isControlPage()) {
                    const int actionCount = controlCount();
                    if (actionCount <= 0) {
                        statusMessage_ = "NO CONTROLS TO REBIND";
                        return;
                    }
                    if (pressedKey == GLFW_KEY_SPACE) {
                        awaitingRebindAction_ = std::clamp(selectedSettingRow_, 0, actionCount - 1);
                        awaitingRebind_ = true;
                        ignoreNextRebindMousePress_ = false;
                        applyRebindCode(GLFW_KEY_SPACE, controls, controlsConfigPath);
                    } else {
                        beginRebindForRow(selectedSettingRow_);
                        ignoreNextRebindMousePress_ = false;
                    }
                } else if (settingsPage_ == SettingsPage::Display && pendingDisplayChanges_ &&
                           (pressedKey == GLFW_KEY_ENTER || pressedKey == GLFW_KEY_KP_ENTER))
                {
                    commitSelectedSetting(window);
                } else {
                    if (isSettingRowDisabled(selectedSettingRow_)) {
                        statusMessage_ = disabledReasonForRow(selectedSettingRow_);
                        return;
                    }
                    if (controlTypeForRow(selectedSettingRow_) == DebugOverlay::PauseMenuControlType::Action) {
                        commitSelectedSetting(window);
                    } else {
                        adjustSelectedSetting(1, window);
                    }
                }
                return;
            default:
                return;
        }
    }

    void PauseMenuController::handlePointerInput(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath,
        const float scrollDeltaY)
    {
        if (window == nullptr) {
            return;
        }

        const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clickPressed = leftMouseDown && !leftMouseWasDown_;
        leftMouseWasDown_ = leftMouseDown;
        int pressedMouseBindingCode = GLFW_KEY_UNKNOWN;
        for (int button = 0; button < input::kMouseBindingMaxButtons; ++button) {
            const bool down = glfwGetMouseButton(window, button) == GLFW_PRESS;
            if (down && !mouseButtonWasDown_[static_cast<std::size_t>(button)] &&
                pressedMouseBindingCode == GLFW_KEY_UNKNOWN)
            {
                pressedMouseBindingCode = input::bindingCodeFromMouseButton(button);
            }
            mouseButtonWasDown_[static_cast<std::size_t>(button)] = down;
        }

        if (!open_) {
            hoveredSettingRow_ = -1;
            hoveredDisplayApplyAction_ = false;
            hoveredResetControlsAction_ = false;
            hoveredResetIcon_ = false;
            hoveredResetConfirmYes_ = false;
            hoveredResetConfirmNo_ = false;
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
        const float menuScale = std::clamp(uiScale(), 0.80f, 1.40f);
        const float baseScalePx = 2.0f * menuScale;
        const float cardW = std::min(w * 0.82f, 1240.0f);
        const float cardH = std::min(h * 0.86f, 920.0f);
        const float cardX0 = (w - cardW) * 0.5f;
        const float cardY0 = (h - cardH) * 0.5f;
        const float cardY1 = cardY0 + cardH;
        const float cardX1 = cardX0 + cardW;

        const float statusSlotY = cardY0 + 96.0f * menuScale;
        const float statusReservedScalePx = baseScalePx * 0.95f;
        const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
        const float tabsSlotY = statusSlotY + statusReservedHeight + 16.0f * menuScale;

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

        const DebugOverlay::PauseMenuHud hud = buildHud(controls, false);
        if (hud.lines.empty()) {
            hoveredSettingRow_ = -1;
            return;
        }

        constexpr float kSettingsScalePx = 2.10f;
        const float footerReservedH = (static_cast<float>(kFontH) + 3.0f) * (baseScalePx * 0.92f) + 18.0f * menuScale;
        float settingsY0 = std::max(cardY0 + 170.0f * menuScale, tabsSlotY + tabHeight + 16.0f * menuScale);
        const float sectionX0 = cardX0 + 26.0f * menuScale;
        const float sectionX1 = cardX1 - 26.0f * menuScale;
        const float settingsY1 = cardY1 - 20.0f * menuScale - footerReservedH;
        if (settingsY1 - settingsY0 < 220.0f) {
            settingsY0 = settingsY1 - 220.0f;
        }

        float rowScalePx = kSettingsScalePx * menuScale * 1.8f;
        float maxRowWidth = 0.0f;
        float maxControlReserve = 0.0f;
        for (const auto& line : hud.lines) {
            maxRowWidth = std::max(maxRowWidth, measureMaxLinePx(line.text, 1.0f));
            if (line.header) {
                continue;
            }
            switch (line.controlType) {
                case DebugOverlay::PauseMenuControlType::Toggle:
                    maxControlReserve = std::max(maxControlReserve, 128.0f * menuScale);
                    break;
                case DebugOverlay::PauseMenuControlType::Choice:
                case DebugOverlay::PauseMenuControlType::Numeric:
                    maxControlReserve = std::max(maxControlReserve, 248.0f * menuScale);
                    break;
                case DebugOverlay::PauseMenuControlType::Rebind:
                    maxControlReserve = std::max(maxControlReserve, 240.0f * menuScale);
                    break;
                case DebugOverlay::PauseMenuControlType::Action:
                    maxControlReserve = std::max(maxControlReserve, 120.0f * menuScale);
                    break;
                default:
                    break;
            }
        }
        const float rowAreaWidth = (sectionX1 - sectionX0) - 28.0f * menuScale - maxControlReserve;
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
        int firstSelectableLine = -1;
        for (std::size_t i = 0; i < hud.lines.size(); ++i) {
            if (!hud.lines[i].header) {
                firstSelectableLine = static_cast<int>(i);
                break;
            }
        }
        if (firstSelectableLine < 0) {
            hoveredSettingRow_ = -1;
            return;
        }
        int hoveredLineIndex = -1;
        int hoveredRowIndex = -1;
        float hoveredLineX1 = 0.0f;
        float hoveredLineY0 = 0.0f;
        float hoveredLineY1 = 0.0f;
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
                hoveredSettingRow_ = -1;
                break;
            }

            const int rowIndex = static_cast<int>(lineIdx) - firstSelectableLine;
            if (rowIndex < 0 || rowIndex >= count) {
                break;
            }
            hoveredLineIndex = static_cast<int>(lineIdx);
            hoveredRowIndex = rowIndex;
            hoveredLineX1 = lineX1;
            hoveredLineY0 = lineY0;
            hoveredLineY1 = lineY1;
            break;
        }

        hoveredSettingRow_ = hoveredRowIndex;
        hoveredDisplayApplyAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;

        if (settingsPage_ == SettingsPage::Display && pendingDisplayChanges_) {
            const std::string applyLabel = "APPLY CHANGES";
            const float actionScalePx = baseScalePx * 0.98f;
            const float actionPadX = 16.0f * menuScale;
            const float actionPadY = 8.0f * menuScale;
            const float actionW = measureMaxLinePx(applyLabel, actionScalePx) + actionPadX * 2.0f;
            const float actionH = (static_cast<float>(kFontH) + 2.0f) * actionScalePx + actionPadY * 2.0f;
            const float actionX1 = sectionX1 - 12.0f * menuScale;
            const float actionX0 = actionX1 - actionW;
            const float actionY1 = settingsY1 - 10.0f * menuScale;
            const float actionY0 = actionY1 - actionH;
            hoveredDisplayApplyAction_ =
                xPx >= actionX0 && xPx <= actionX1 && yPx >= actionY0 && yPx <= actionY1;
        }
        if (hud.showResetControlsAction) {
            const std::string resetLabel = "RESET CONTROLS";
            const float actionScalePx = baseScalePx * 0.98f;
            const float actionPadX = 16.0f * menuScale;
            const float actionPadY = 8.0f * menuScale;
            const float actionW = measureMaxLinePx(resetLabel, actionScalePx) + actionPadX * 2.0f;
            const float actionH = (static_cast<float>(kFontH) + 2.0f) * actionScalePx + actionPadY * 2.0f;
            const float actionX1 = sectionX1 - 12.0f * menuScale;
            const float actionX0 = actionX1 - actionW;
            const float actionY1 = settingsY1 - 10.0f * menuScale;
            const float actionY0 = actionY1 - actionH;
            hoveredResetControlsAction_ =
                xPx >= actionX0 && xPx <= actionX1 && yPx >= actionY0 && yPx <= actionY1;
        }

        if (awaitingRebind_) {
            if (pressedMouseBindingCode == GLFW_KEY_UNKNOWN) {
                return;
            }
            if (ignoreNextRebindMousePress_) {
                ignoreNextRebindMousePress_ = false;
                return;
            }
            if (hoveredRowIndex != selectedSettingRow_)
            {
                awaitingRebind_ = false;
                awaitingRebindAction_ = -1;
                ignoreNextRebindMousePress_ = false;
                statusMessage_ = "REBIND CANCELED";
                return;
            }
            applyRebindCode(pressedMouseBindingCode, controls, controlsConfigPath);
            return;
        }

        if (std::fabs(scrollDeltaY) > 0.01f) {
            const int wheelDirection = scrollDeltaY > 0.0f ? 1 : -1;
            const int wheelSteps = std::min(4, std::max(1, static_cast<int>(std::fabs(scrollDeltaY))));
            if (hoveredRowIndex >= 0) {
                if (selectedSettingRow_ != hoveredRowIndex) {
                    if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                        discardPendingEdit();
                    }
                    setSelectedSettingRow(hoveredRowIndex);
                }
                const auto controlType = controlTypeForRow(hoveredRowIndex);
                if (!isControlPage() &&
                    !isSettingRowDisabled(hoveredRowIndex) &&
                    (controlType == DebugOverlay::PauseMenuControlType::Numeric ||
                     controlType == DebugOverlay::PauseMenuControlType::Choice))
                {
                    for (int i = 0; i < wheelSteps; ++i) {
                        adjustSelectedSetting(wheelDirection, window);
                    }
                    return;
                }
            }
            for (int i = 0; i < wheelSteps; ++i) {
                if (wheelDirection > 0) {
                    selectPrev();
                } else {
                    selectNext();
                }
            }
        }

        if (!clickPressed) {
            return;
        }

        const double clickAt = glfwGetTime();
        constexpr double kDoubleClickWindowSeconds = 0.35;

        const float buttonPadX = 12.0f * menuScale;
        const std::string exitLabel = "EXIT TO HOME";
        const float exitWidth = measureMaxLinePx(exitLabel, tabsScalePx) + buttonPadX * 2.0f;
        const float exitX1 = cardX1 - 18.0f * menuScale;
        const float exitX0 = exitX1 - exitWidth;
        const float exitY0 = tabsSlotY;
        const float exitY1 = exitY0 + tabHeight;
        const std::string closeLabel = "X";
        const float closePadX = 12.0f * menuScale;
        const float closeWidth = measureMaxLinePx(closeLabel, tabsScalePx) + closePadX * 2.0f;
        const float closeX1 = cardX1 - 18.0f * menuScale;
        const float closeX0 = closeX1 - closeWidth;
        const float closeY0 = cardY0 + 18.0f * menuScale;
        const float closeY1 = closeY0 + tabHeight;
        const std::string resetIconLabel = "R";
        const float resetPadX = 12.0f * menuScale;
        const float resetWidth = measureMaxLinePx(resetIconLabel, tabsScalePx) + resetPadX * 2.0f;
        const float resetX0 = cardX0 + 18.0f * menuScale;
        const float resetX1 = resetX0 + resetWidth;
        const float resetY0 = cardY0 + 18.0f * menuScale;
        const float resetY1 = resetY0 + tabHeight;

        if (xPx >= closeX0 && xPx <= closeX1 && yPx >= closeY0 && yPx <= closeY1) {
            resumeRequested_ = true;
            return;
        }
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

        hoveredResetIcon_ = xPx >= resetX0 && xPx <= resetX1 && yPx >= resetY0 && yPx <= resetY1;

        if (!confirmingResetSettings_ && hoveredResetIcon_) {
            if (clickPressed) {
                confirmingResetSettings_ = true;
            }
            return;
        }

        if (confirmingResetSettings_) {
            const float popupW = 320.0f * menuScale;
            const float popupH = 120.0f * menuScale;
            const float popupX0 = cardX0 + (cardW - popupW) * 0.5f;
            const float popupY0 = cardY0 + (cardH - popupH) * 0.5f;
            const float popupX1 = popupX0 + popupW;
            const float popupY1 = popupY0 + popupH;
            const float buttonScalePx = baseScalePx * 0.98f;
            const float buttonPadX = 16.0f * menuScale;
            const float buttonPadY = 8.0f * menuScale;
            const std::string yesLabel = "RESET";
            const std::string noLabel = "CANCEL";
            const float yesW = measureMaxLinePx(yesLabel, buttonScalePx) + buttonPadX * 2.0f;
            const float noW = measureMaxLinePx(noLabel, buttonScalePx) + buttonPadX * 2.0f;
            const float buttonH = (static_cast<float>(kFontH) + 2.0f) * buttonScalePx + buttonPadY * 2.0f;
            const float buttonsY1 = popupY1 - 14.0f * menuScale;
            const float buttonsY0 = buttonsY1 - buttonH;
            const float noX1 = popupX1 - 18.0f * menuScale;
            const float noX0 = noX1 - noW;
            const float yesX1 = noX0 - 10.0f * menuScale;
            const float yesX0 = yesX1 - yesW;
            hoveredResetConfirmYes_ =
                xPx >= yesX0 && xPx <= yesX1 && yPx >= buttonsY0 && yPx <= buttonsY1;
            hoveredResetConfirmNo_ =
                xPx >= noX0 && xPx <= noX1 && yPx >= buttonsY0 && yPx <= buttonsY1;

            if (clickPressed) {
                if (hoveredResetConfirmYes_) {
                    resetAllSettings(window);
                    confirmingResetSettings_ = false;
                } else if (hoveredResetConfirmNo_ ||
                           !(xPx >= popupX0 && xPx <= popupX1 && yPx >= popupY0 && yPx <= popupY1))
                {
                    confirmingResetSettings_ = false;
                }
            }
            return;
        }

        if (hoveredRowIndex < 0 || hoveredLineIndex < 0) {
            if (hoveredDisplayApplyAction_) {
                if (clickPressed) {
                    commitSelectedSetting(window);
                }
                return;
            }
            if (hoveredResetControlsAction_) {
                if (clickPressed) {
                    controls = input::ControlBindings{};
                    input::saveControlBindings(controls, controlsConfigPath);
                    markAppliedSelection();
                    statusMessage_.clear();
                }
                return;
            }
            if (selectedSettingRow_ >= 0 &&
                xPx >= cardX0 && xPx <= cardX1 &&
                yPx >= settingsY0 && yPx <= settingsY1)
            {
                if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                    discardPendingEdit();
                }
                setSelectedSettingRow(-1);
                statusMessage_.clear();
            }
            return;
        }

        const bool isDoubleClick =
            lastClickedPage_ == settingsPage_ &&
            lastClickedRow_ == hoveredRowIndex &&
            lastClickAt_ >= 0.0 &&
            (clickAt - lastClickAt_) <= kDoubleClickWindowSeconds;
        lastClickedPage_ = settingsPage_;
        lastClickedRow_ = hoveredRowIndex;
        lastClickAt_ = clickAt;

        if (selectedSettingRow_ != hoveredRowIndex) {
            if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
                discardPendingEdit();
            }
            setSelectedSettingRow(hoveredRowIndex);
            statusMessage_.clear();
        }

        if (isControlPage()) {
            if (!isDoubleClick) {
                return;
            }
            if (beginRebindForRow(hoveredRowIndex)) {
                ignoreNextRebindMousePress_ = true;
            }
            return;
        }

        if (isSettingRowDisabled(hoveredRowIndex) || hud.lines[hoveredLineIndex].disabled) {
            statusMessage_ = disabledReasonForRow(hoveredRowIndex);
            return;
        }

        const DebugOverlay::PauseMenuControlType controlType = controlTypeForRow(hoveredRowIndex);
        if (controlType == DebugOverlay::PauseMenuControlType::Toggle) {
            const float valueW = 220.0f * menuScale;
            const float rightX1 = hoveredLineX1 - 10.0f * menuScale;
            const float valueX1 = rightX1 - 24.0f * menuScale - 6.0f * menuScale;
            const float x1 = valueX1;
            const float x0 = x1 - valueW;
            const float box = std::max(8.0f, (hoveredLineY1 - hoveredLineY0) - 8.0f * menuScale);
            const float bx0 = x0 + 18.0f * menuScale;
            const float by0 = hoveredLineY0 + (hoveredLineY1 - hoveredLineY0 - box) * 0.5f;
            const float bx1 = bx0 + box;
            const float by1 = by0 + box;
            if (xPx >= bx0 && xPx <= bx1 && yPx >= by0 && yPx <= by1) {
                adjustSelectedSetting(1, window);
                return;
            }
        }

        if (controlType == DebugOverlay::PauseMenuControlType::Choice ||
            controlType == DebugOverlay::PauseMenuControlType::Numeric)
        {
            const float arrowW = 24.0f * menuScale;
            const float gap = 6.0f * menuScale;
            const float valueW = 220.0f * menuScale;
            const float rightX1 = hoveredLineX1 - 10.0f * menuScale;
            const float rightArrowX0 = rightX1 - arrowW;
            const float valueX1 = rightArrowX0 - gap;
            const float valueX0 = valueX1 - valueW;
            const float leftArrowX1 = valueX0 - gap;
            const float leftArrowX0 = leftArrowX1 - arrowW;

            if (xPx >= leftArrowX0 && xPx <= leftArrowX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1) {
                adjustSelectedSetting(-1, window);
                return;
            }
            if (xPx >= rightArrowX0 && xPx <= rightX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1) {
                adjustSelectedSetting(1, window);
                return;
            }
        }

        if (!isDoubleClick) {
            return;
        }

        if (controlType == DebugOverlay::PauseMenuControlType::Toggle) {
            adjustSelectedSetting(1, window);
            return;
        }

        if (controlType == DebugOverlay::PauseMenuControlType::Choice ||
            controlType == DebugOverlay::PauseMenuControlType::Numeric)
        {
            const float arrowW = 24.0f * menuScale;
            const float gap = 6.0f * menuScale;
            const float valueW = 220.0f * menuScale;
            const float rightX1 = hoveredLineX1 - 10.0f * menuScale;
            const float rightArrowX0 = rightX1 - arrowW;
            const float valueX1 = rightArrowX0 - gap;
            const float valueX0 = valueX1 - valueW;
            const float leftArrowX1 = valueX0 - gap;
            const float leftArrowX0 = leftArrowX1 - arrowW;
            const float sliderX0 = valueX0 + 6.0f * menuScale;
            const float sliderX1 = valueX1 - 6.0f * menuScale;
            const float sliderY1 = hoveredLineY1 - 3.0f * menuScale;
            const float sliderY0 = sliderY1 - 6.0f * menuScale;

            if (controlType == DebugOverlay::PauseMenuControlType::Numeric &&
                xPx >= sliderX0 && xPx <= sliderX1 && yPx >= sliderY0 && yPx <= sliderY1)
            {
                const float width = std::max(1.0f, sliderX1 - sliderX0);
                const float t = std::clamp((xPx - sliderX0) / width, 0.0f, 1.0f);
                const int optionCount = optionCountForRow(hoveredRowIndex);
                if (optionCount > 1) {
                    const int target = std::clamp(
                        static_cast<int>(std::round(t * static_cast<float>(optionCount - 1))),
                        0,
                        optionCount - 1);
                    setOptionIndexForRow(hoveredRowIndex, target, window);
                }
                return;
            }
            if (xPx >= valueX0 && xPx <= valueX1 && yPx >= hoveredLineY0 && yPx <= hoveredLineY1) {
                adjustSelectedSetting(1, window);
                return;
            }
            return;
        }

        if (controlType == DebugOverlay::PauseMenuControlType::Action) {
            if (settingsPage_ == SettingsPage::Interface && hoveredRowIndex == interfaceSettingRowCount() - 1) {
                resetAllSettings(window);
            } else {
                commitSelectedSetting(window);
            }
            return;
        }

        if (controlType == DebugOverlay::PauseMenuControlType::None) {
            adjustSelectedSetting(1, window);
            return;
        }

        adjustSelectedSetting(1, window);
    }

    void PauseMenuController::updateContinuousInput(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        (void)controls;
        (void)controlsConfigPath;
        if (!open_ || window == nullptr || awaitingRebind_) {
            upHeld_ = false;
            downHeld_ = false;
            leftHeld_ = false;
            rightHeld_ = false;
            return;
        }

        const double now = glfwGetTime();
        constexpr double kInitialDelay = 0.32;
        constexpr double kRepeatInterval = 0.06;

        const auto stepRepeat = [&](const int key,
                                    bool& held,
                                    double& nextRepeatAt,
                                    const auto& action) {
            const bool down = glfwGetKey(window, key) == GLFW_PRESS;
            if (!down) {
                held = false;
                nextRepeatAt = 0.0;
                return;
            }
            if (!held) {
                held = true;
                nextRepeatAt = now + kInitialDelay;
                return;
            }
            if (now < nextRepeatAt) {
                return;
            }
            action();
            nextRepeatAt = now + kRepeatInterval;
        };

        stepRepeat(GLFW_KEY_UP, upHeld_, upNextRepeatAt_, [&]() {
            selectPrev();
        });
        stepRepeat(GLFW_KEY_DOWN, downHeld_, downNextRepeatAt_, [&]() {
            selectNext();
        });

        if (!isControlPage() &&
            !isSettingRowDisabled(selectedSettingRow_) &&
            supportsContinuousAdjust(selectedSettingRow_))
        {
            stepRepeat(GLFW_KEY_LEFT, leftHeld_, leftNextRepeatAt_, [&]() {
                adjustSelectedSetting(-1, window);
            });
            stepRepeat(GLFW_KEY_RIGHT, rightHeld_, rightNextRepeatAt_, [&]() {
                adjustSelectedSetting(1, window);
            });
        } else {
            leftHeld_ = false;
            rightHeld_ = false;
            leftNextRepeatAt_ = 0.0;
            rightNextRepeatAt_ = 0.0;
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
        hud.showDisplayApplyAction =
            settingsPage_ == SettingsPage::Display && pendingDisplayChanges_;
        hud.hoverDisplayApplyAction = hoveredDisplayApplyAction_;
        hud.showResetControlsAction = settingsPage_ == SettingsPage::Controls && hasControlChanges(controls);
        hud.hoverResetControlsAction = hoveredResetControlsAction_;
        hud.showResetIcon = true;
        hud.hoverResetIcon = hoveredResetIcon_;
        hud.showResetConfirm = confirmingResetSettings_;
        hud.hoverResetConfirmYes = hoveredResetConfirmYes_;
        hud.hoverResetConfirmNo = hoveredResetConfirmNo_;
        hud.footerHint =
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   ENTER: APPLY OR REBIND   TAB: CHANGE PAGE   ESC: CLOSE";

        if (awaitingRebind_ && awaitingRebindAction_ >= 0 && awaitingRebindAction_ < controlCount()) {
            hud.pendingAction = input::rebindActions()[awaitingRebindAction_].label;
        }

        hud.statusLine = statusMessage_;
        hud.activePageIndex = static_cast<int>(settingsPage_);
        if (hud.statusLine.empty()) {
            const int rowForHint = hoveredSettingRow_ >= 0 ? hoveredSettingRow_ : selectedSettingRow_;
            if (!isControlPage() && rowForHint >= 0 && isSettingRowDisabled(rowForHint)) {
                hud.statusLine = disabledReasonForRow(rowForHint);
            }
        }

        int appliedRowOnCurrentPage = -1;
        if (applyIndicatorFrames_ > 0 && lastAppliedPage_ == settingsPage_) {
            appliedRowOnCurrentPage = lastAppliedRow_;
        }

        const DisplaySettings& displayRef =
            (settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)
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

        const auto normalized = [](const int idx, const int count) {
            if (count <= 1) {
                return 0.0f;
            }
            return std::clamp(static_cast<float>(idx) / static_cast<float>(count - 1), 0.0f, 1.0f);
        };
        const auto addHeader = [&](const std::string& title) {
            DebugOverlay::PauseMenuHudLine line{};
            line.text = title;
            line.header = true;
            hud.lines.push_back(line);
        };
        const auto addRow = [&](const std::string& label,
                                const std::string& value,
                                const bool disabled,
                                const DebugOverlay::PauseMenuControlType type,
                                const bool boolValue,
                                const bool showArrows,
                                const bool showSlider,
                                const float sliderT,
                                const std::string& hint = std::string()) {
            DebugOverlay::PauseMenuHudLine line{};
            line.text = label;
            line.valueText = value;
            line.hintText = hint;
            line.header = false;
            line.disabled = disabled;
            line.controlType = type;
            line.boolValue = boolValue;
            line.showArrows = showArrows;
            line.showSlider = showSlider;
            line.sliderT = sliderT;
            hud.lines.push_back(line);
        };

        int firstSelectableLine = 1;
        int rowCount = settingCount();

        if (settingsPage_ == SettingsPage::Display) {
            addHeader("DISPLAY SETTINGS");
            addRow(
                "WINDOW MODE",
                windowModeLabel(displayRef.windowMode),
                false,
                DebugOverlay::PauseMenuControlType::Choice,
                false,
                true,
                false,
                0.0f,
                "CYCLE WITH LEFT/RIGHT");
            const bool resolutionLocked = displayRef.windowMode == WindowMode::Borderless;
            const std::string resolutionValue = resolutionLocked
                ? "LOCKED"
                : resolutionLabel(displayRef.resolutionIndex);
            addRow(
                "RESOLUTION",
                resolutionValue,
                resolutionLocked,
                DebugOverlay::PauseMenuControlType::Choice,
                false,
                true,
                false,
                normalized(displayRef.resolutionIndex, resolutionChoiceCount()),
                resolutionLocked ? "LOCKED IN BORDERLESS MODE" : "CYCLE WITH LEFT/RIGHT");
            addRow(
                "VSYNC",
                displayRef.vsync ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                displayRef.vsync,
                false,
                false,
                displayRef.vsync ? 1.0f : 0.0f,
                "TOGGLE");

            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex =
                    1 + std::clamp(selectedSettingRow_, 0, displaySettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, displaySettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Simulation) {
            addHeader("SIMULATION SETTINGS");
            const int minSpeedIdx = closestChoiceIndex(kMinSpeedChoices, simRef.minSimSpeed);
            const int maxSpeedIdx = closestChoiceIndex(kMaxSpeedChoices, simRef.maxSimSpeed);
            addRow(
                "MIN SPEED",
                formatSpeedMultiplier(simRef.minSimSpeed),
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(minSpeedIdx, static_cast<int>(kMinSpeedChoices.size())),
                "ARROWS / WHEEL / SLIDER");
            addRow(
                "MAX SPEED",
                formatSpeedMultiplier(simRef.maxSimSpeed),
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(maxSpeedIdx, static_cast<int>(kMaxSpeedChoices.size())),
                "ARROWS / WHEEL / SLIDER");
            const int gravityStrengthIdx = closestChoiceIndex(kGravityStrengthChoices, simRef.gravityStrength);
            addRow(
                "GRAVITY",
                simRef.gravityEnabled ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                simRef.gravityEnabled,
                false,
                false,
                simRef.gravityEnabled ? 1.0f : 0.0f,
                "TOGGLE");
            addRow(
                "GRAVITY STRENGTH",
                formatGravityMultiplier(simRef.gravityStrength),
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(gravityStrengthIdx, static_cast<int>(kGravityStrengthChoices.size())),
                "ARROWS / WHEEL / SLIDER");
            const int collisionIterationsIdx =
                closestChoiceIndex(kCollisionIterationChoices, simRef.collisionIterations);
            addRow(
                "COLLISION ITERATIONS",
                std::to_string(simRef.collisionIterations),
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(collisionIterationsIdx, static_cast<int>(kCollisionIterationChoices.size())),
                "ARROWS / WHEEL / SLIDER");

            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex =
                    1 + std::clamp(selectedSettingRow_, 0, simulationSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, simulationSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            addHeader("CAMERA SETTINGS");
            char sensitivityBuffer[32];
            std::snprintf(sensitivityBuffer, sizeof(sensitivityBuffer), "%.4f", cameraRef.lookSensitivity);
            const int sensitivityIdx = closestChoiceIndex(kLookSensitivityChoices, cameraRef.lookSensitivity);
            addRow(
                "LOOK SENSITIVITY",
                sensitivityBuffer,
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(sensitivityIdx, static_cast<int>(kLookSensitivityChoices.size())),
                "ARROWS / WHEEL / SLIDER");

            char moveSpeedBuffer[32];
            std::snprintf(moveSpeedBuffer, sizeof(moveSpeedBuffer), "%.0f", cameraRef.baseMoveSpeed);
            const int moveSpeedIdx = closestChoiceIndex(kBaseMoveSpeedChoices, cameraRef.baseMoveSpeed);
            addRow(
                "BASE MOVE SPEED",
                moveSpeedBuffer,
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(moveSpeedIdx, static_cast<int>(kBaseMoveSpeedChoices.size())),
                "ARROWS / WHEEL / SLIDER");

            addRow(
                "INVERT Y",
                cameraRef.invertY ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                cameraRef.invertY,
                false,
                false,
                cameraRef.invertY ? 1.0f : 0.0f,
                "TOGGLE");

            char fovBuffer[32];
            std::snprintf(fovBuffer, sizeof(fovBuffer), "%.0f DEG", cameraRef.fovDegrees);
            const int fovIdx = closestChoiceIndex(kFovChoices, cameraRef.fovDegrees);
            addRow(
                "FOV",
                fovBuffer,
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(fovIdx, static_cast<int>(kFovChoices.size())),
                "ARROWS / WHEEL / SLIDER");

            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex =
                    1 + std::clamp(selectedSettingRow_, 0, cameraSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, cameraSettingRowCount() - 1);
            }
        } else if (settingsPage_ == SettingsPage::Interface) {
            addHeader("INTERFACE");

            char scaleBuffer[32];
            std::snprintf(scaleBuffer, sizeof(scaleBuffer), "%.2fX", uiScaleValue(interfaceRef.uiScaleIndex));
            addRow(
                "UI SCALE",
                scaleBuffer,
                false,
                DebugOverlay::PauseMenuControlType::Numeric,
                false,
                true,
                true,
                normalized(interfaceRef.uiScaleIndex, static_cast<int>(kUiScaleChoices.size())),
                "ARROWS / WHEEL / SLIDER");
            addRow(
                "SHOW HUD",
                interfaceRef.showHud ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.showHud,
                false,
                false,
                interfaceRef.showHud ? 1.0f : 0.0f,
                "TOGGLE");
            addRow(
                "CROSSHAIR",
                interfaceRef.showCrosshair ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.showCrosshair,
                false,
                false,
                interfaceRef.showCrosshair ? 1.0f : 0.0f,
                "TOGGLE");
            addRow(
                "OBJECT INFO",
                interfaceRef.objectInfo ? "ON" : "OFF",
                false,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.objectInfo,
                false,
                false,
                interfaceRef.objectInfo ? 1.0f : 0.0f,
                "TOGGLE");
            addRow(
                "OBJECT INFO MATERIAL",
                interfaceRef.objectInfoMaterial ? "ON" : "OFF",
                !interfaceRef.objectInfo,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.objectInfoMaterial,
                false,
                false,
                interfaceRef.objectInfoMaterial ? 1.0f : 0.0f,
                !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow(
                "OBJECT INFO VELOCITY",
                interfaceRef.objectInfoVelocity ? "ON" : "OFF",
                !interfaceRef.objectInfo,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.objectInfoVelocity,
                false,
                false,
                interfaceRef.objectInfoVelocity ? 1.0f : 0.0f,
                !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow(
                "OBJECT INFO MASS",
                interfaceRef.objectInfoMass ? "ON" : "OFF",
                !interfaceRef.objectInfo,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.objectInfoMass,
                false,
                false,
                interfaceRef.objectInfoMass ? 1.0f : 0.0f,
                !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            addRow(
                "OBJECT INFO RADIUS",
                interfaceRef.objectInfoRadius ? "ON" : "OFF",
                !interfaceRef.objectInfo,
                DebugOverlay::PauseMenuControlType::Toggle,
                interfaceRef.objectInfoRadius,
                false,
                false,
                interfaceRef.objectInfoRadius ? 1.0f : 0.0f,
                !interfaceRef.objectInfo ? "ENABLE OBJECT INFO FIRST" : "TOGGLE");
            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex =
                    1 + std::clamp(selectedSettingRow_, 0, interfaceSettingRowCount() - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    1 + std::clamp(appliedRowOnCurrentPage, 0, interfaceSettingRowCount() - 1);
            }
        } else {
            addHeader("CONTROLS");
            firstSelectableLine = static_cast<int>(hud.lines.size());
            rowCount = controlCount();
            const int actionCount = controlCount();
            for (int i = 0; i < actionCount; ++i) {
                const auto& action = input::rebindActions()[i];
                const int keyCode = controls.*(action.member);
                addRow(
                    action.label,
                    input::keyNameForCode(keyCode),
                    false,
                    DebugOverlay::PauseMenuControlType::Rebind,
                    false,
                    false,
                    false,
                    0.0f,
                    "ENTER/SPACE OR CLICK TO REBIND");
            }

            const int controlsPageRows = actionCount;
            if (selectedSettingRow_ >= 0) {
                hud.selectedSettingLineIndex =
                    firstSelectableLine + std::clamp(selectedSettingRow_, 0, controlsPageRows - 1);
            }
            if (appliedRowOnCurrentPage >= 0) {
                hud.appliedSettingLineIndex =
                    firstSelectableLine + std::clamp(appliedRowOnCurrentPage, 0, controlsPageRows - 1);
            }
        }

        if (rowCount > 0 && hoveredSettingRow_ >= 0) {
            hud.hoveredSettingLineIndex =
                firstSelectableLine + std::clamp(hoveredSettingRow_, 0, rowCount - 1);
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
                return controlCount();
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
        return 5;
    }

    int PauseMenuController::cameraSettingRowCount() {
        return 4;
    }

    int PauseMenuController::interfaceSettingRowCount() {
        return 8;
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
        return value;
    }

    std::string PauseMenuController::formatMenuValue(const std::string& value) {
        return value;
    }

    bool PauseMenuController::isControlPage() const {
        return settingsPage_ == SettingsPage::Controls;
    }

    bool PauseMenuController::hasControlChanges(const input::ControlBindings& controls) {
        const input::ControlBindings defaults{};
        return controls.freeze != defaults.freeze ||
               controls.speedDown != defaults.speedDown ||
               controls.speedUp != defaults.speedUp ||
               controls.speedReset != defaults.speedReset ||
               controls.moveForward != defaults.moveForward ||
               controls.moveBack != defaults.moveBack ||
               controls.moveLeft != defaults.moveLeft ||
               controls.moveRight != defaults.moveRight ||
               controls.moveUp != defaults.moveUp ||
               controls.moveDown != defaults.moveDown ||
               controls.cameraBoost != defaults.cameraBoost;
    }

    void PauseMenuController::setSelectedSettingRow(const int row) {
        selectedSettingRow_ = row;
    }

    void PauseMenuController::discardPendingEdit() {
        draftDisplaySettings_ = appliedDisplaySettings_;
        draftSimulationSettings_ = appliedSimulationSettings_;
        draftCameraSettings_ = appliedCameraSettings_;
        draftInterfaceSettings_ = appliedInterfaceSettings_;
        pendingDisplayChanges_ = false;
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
        ignoreNextRebindMousePress_ = false;
        hoveredSettingRow_ = -1;
        hoveredDisplayApplyAction_ = false;
        hoveredResetControlsAction_ = false;
        hoveredResetIcon_ = false;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;

        setSelectedSettingRow(-1);
        statusMessage_.clear();
    }

    void PauseMenuController::commitSelectedSetting(GLFWwindow* window) {
        if (isControlPage()) {
            return;
        }

        if (settingsPage_ == SettingsPage::Display) {
            if (!pendingDisplayChanges_) {
                statusMessage_.clear();
                return;
            }
            appliedDisplaySettings_ = draftDisplaySettings_;
            applyDisplaySettings(window, appliedDisplaySettings_);
        } else if (settingsPage_ == SettingsPage::Simulation) {
            if (!pendingSelectionEdit_ ||
                draftSettingsPage_ != settingsPage_ ||
                draftSelectionRow_ != selectedSettingRow_)
            {
                statusMessage_.clear();
                return;
            }
            appliedSimulationSettings_ = draftSimulationSettings_;
        } else if (settingsPage_ == SettingsPage::Camera) {
            if (!pendingSelectionEdit_ ||
                draftSettingsPage_ != settingsPage_ ||
                draftSelectionRow_ != selectedSettingRow_)
            {
                statusMessage_.clear();
                return;
            }
            appliedCameraSettings_ = draftCameraSettings_;
        } else if (settingsPage_ == SettingsPage::Interface) {
            if (!pendingSelectionEdit_ ||
                draftSettingsPage_ != settingsPage_ ||
                draftSelectionRow_ != selectedSettingRow_)
            {
                statusMessage_.clear();
                return;
            }
            appliedInterfaceSettings_ = draftInterfaceSettings_;
        }
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();

        pendingDisplayChanges_ = false;
        pendingSelectionEdit_ = false;
        draftSelectionRow_ = -1;
        draftSettingsPage_ = settingsPage_;
    }

    bool PauseMenuController::beginRebindForRow(const int row) {
        if (!isControlPage()) {
            return false;
        }
        const int actionCount = controlCount();
        if (actionCount <= 0) {
            statusMessage_ = "NO CONTROLS TO REBIND";
            return false;
        }
        if (row < 0 || row >= actionCount) {
            return false;
        }
        awaitingRebindAction_ = row;
        awaitingRebind_ = true;
        ignoreNextRebindMousePress_ = false;
        statusMessage_ = "PRESS A KEY OR MOUSE BUTTON...  ESC/BACK/CANCEL TO ABORT";
        return true;
    }

    bool PauseMenuController::applyRebindCode(
        const int code,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        if (awaitingRebindAction_ < 0 || awaitingRebindAction_ >= controlCount()) {
            awaitingRebind_ = false;
            awaitingRebindAction_ = -1;
            ignoreNextRebindMousePress_ = false;
            statusMessage_ = "REBIND ERROR";
            return false;
        }

        if (!input::isBindableKey(code)) {
            statusMessage_ = "INPUT NOT BINDABLE";
            return false;
        }

        const auto& actions = input::rebindActions();
        for (int actionIndex = 0; actionIndex < controlCount(); ++actionIndex) {
            if (actionIndex == awaitingRebindAction_) {
                continue;
            }
            if (controls.*(actions[actionIndex].member) == code) {
                statusMessage_ = "INPUT ALREADY IN USE";
                return false;
            }
        }

        controls.*(actions[awaitingRebindAction_].member) = code;
        input::saveControlBindings(controls, controlsConfigPath);

        statusMessage_.clear();
        markAppliedSelection();
        awaitingRebind_ = false;
        awaitingRebindAction_ = -1;
        ignoreNextRebindMousePress_ = false;
        return true;
    }

    void PauseMenuController::selectNext() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
            discardPendingEdit();
        }
        setSelectedSettingRow(wrapSelectionRow(selectedSettingRow_ + 1, count));
        statusMessage_.clear();
    }

    void PauseMenuController::selectPrev() {
        const int count = settingCount();
        if (count <= 0) {
            return;
        }

        if (!(settingsPage_ == SettingsPage::Display && pendingDisplayChanges_)) {
            discardPendingEdit();
        }
        setSelectedSettingRow(wrapSelectionRow(selectedSettingRow_ - 1, count));
        statusMessage_.clear();
    }

    bool PauseMenuController::isSettingRowDisabled(const int row) const {
        if (row < 0) {
            return true;
        }
        if (settingsPage_ == SettingsPage::Display) {
            if (row == 1) {
                const WindowMode effectiveMode =
                    (pendingDisplayChanges_ && settingsPage_ == SettingsPage::Display)
                    ? draftDisplaySettings_.windowMode
                    : appliedDisplaySettings_.windowMode;
                return effectiveMode == WindowMode::Borderless;
            }
            return false;
        }
        if (settingsPage_ == SettingsPage::Interface) {
            return row >= 4 && row <= 7 && !appliedInterfaceSettings_.objectInfo;
        }
        return false;
    }

    std::string PauseMenuController::disabledReasonForRow(const int row) const {
        if (settingsPage_ == SettingsPage::Display && row == 1) {
            return "RESOLUTION LOCKED";
        }
        if (settingsPage_ == SettingsPage::Interface && row >= 4 && row <= 7) {
            return "ENABLE OBJECT INFO FIRST";
        }
        return "SETTING LOCKED";
    }

    DebugOverlay::PauseMenuControlType PauseMenuController::controlTypeForRow(const int row) const {
        if (row < 0) {
            return DebugOverlay::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Display) {
            if (row == 2) {
                return DebugOverlay::PauseMenuControlType::Toggle;
            }
            return row <= 1 ? DebugOverlay::PauseMenuControlType::Choice : DebugOverlay::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            if (row == 2) {
                return DebugOverlay::PauseMenuControlType::Toggle;
            }
            return row <= 4 ? DebugOverlay::PauseMenuControlType::Numeric : DebugOverlay::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Camera) {
            if (row == 2) {
                return DebugOverlay::PauseMenuControlType::Toggle;
            }
            return row <= 3 ? DebugOverlay::PauseMenuControlType::Numeric : DebugOverlay::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Interface) {
            if (row == 0) {
                return DebugOverlay::PauseMenuControlType::Numeric;
            }
            if (row <= 7) {
                return DebugOverlay::PauseMenuControlType::Toggle;
            }
            return DebugOverlay::PauseMenuControlType::None;
        }
        if (settingsPage_ == SettingsPage::Controls) {
            return row < controlCount() ? DebugOverlay::PauseMenuControlType::Rebind
                                        : DebugOverlay::PauseMenuControlType::Action;
        }
        return DebugOverlay::PauseMenuControlType::None;
    }

    bool PauseMenuController::supportsContinuousAdjust(const int row) const {
        const auto type = controlTypeForRow(row);
        return type == DebugOverlay::PauseMenuControlType::Numeric ||
               type == DebugOverlay::PauseMenuControlType::Choice;
    }

    int PauseMenuController::optionCountForRow(const int row) const {
        if (row < 0) {
            return 0;
        }
        if (settingsPage_ == SettingsPage::Display) {
            switch (row) {
                case 0: return 3;
                case 1: return resolutionChoiceCount();
                case 2: return 2;
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            switch (row) {
                case 0: return static_cast<int>(kMinSpeedChoices.size());
                case 1: return static_cast<int>(kMaxSpeedChoices.size());
                case 2: return 2;
                case 3: return static_cast<int>(kGravityStrengthChoices.size());
                case 4: return static_cast<int>(kCollisionIterationChoices.size());
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Camera) {
            switch (row) {
                case 0: return static_cast<int>(kLookSensitivityChoices.size());
                case 1: return static_cast<int>(kBaseMoveSpeedChoices.size());
                case 2: return 2;
                case 3: return static_cast<int>(kFovChoices.size());
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Interface) {
            if (row == 0) {
                return static_cast<int>(kUiScaleChoices.size());
            }
            if (row >= 1 && row <= 7) {
                return 2;
            }
        }
        return 0;
    }

    int PauseMenuController::optionIndexForRow(const int row) const {
        if (row < 0) {
            return 0;
        }
        if (settingsPage_ == SettingsPage::Display) {
            const DisplaySettings& displayRef =
                (pendingDisplayChanges_ && settingsPage_ == SettingsPage::Display)
                ? draftDisplaySettings_
                : appliedDisplaySettings_;
            switch (row) {
                case 0: return static_cast<int>(displayRef.windowMode);
                case 1: return std::clamp(displayRef.resolutionIndex, 0, resolutionChoiceCount() - 1);
                case 2: return displayRef.vsync ? 1 : 0;
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Simulation) {
            switch (row) {
                case 0: return closestChoiceIndex(kMinSpeedChoices, appliedSimulationSettings_.minSimSpeed);
                case 1: return closestChoiceIndex(kMaxSpeedChoices, appliedSimulationSettings_.maxSimSpeed);
                case 2: return appliedSimulationSettings_.gravityEnabled ? 1 : 0;
                case 3: return closestChoiceIndex(kGravityStrengthChoices, appliedSimulationSettings_.gravityStrength);
                case 4: return closestChoiceIndex(kCollisionIterationChoices, appliedSimulationSettings_.collisionIterations);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Camera) {
            switch (row) {
                case 0: return closestChoiceIndex(kLookSensitivityChoices, appliedCameraSettings_.lookSensitivity);
                case 1: return closestChoiceIndex(kBaseMoveSpeedChoices, appliedCameraSettings_.baseMoveSpeed);
                case 2: return appliedCameraSettings_.invertY ? 1 : 0;
                case 3: return closestChoiceIndex(kFovChoices, appliedCameraSettings_.fovDegrees);
                default: return 0;
            }
        }
        if (settingsPage_ == SettingsPage::Interface) {
            switch (row) {
                case 0:
                    return std::clamp(
                        appliedInterfaceSettings_.uiScaleIndex,
                        0,
                        static_cast<int>(kUiScaleChoices.size()) - 1);
                case 1: return appliedInterfaceSettings_.showHud ? 1 : 0;
                case 2: return appliedInterfaceSettings_.showCrosshair ? 1 : 0;
                case 3: return appliedInterfaceSettings_.objectInfo ? 1 : 0;
                case 4: return appliedInterfaceSettings_.objectInfoMaterial ? 1 : 0;
                case 5: return appliedInterfaceSettings_.objectInfoVelocity ? 1 : 0;
                case 6: return appliedInterfaceSettings_.objectInfoMass ? 1 : 0;
                case 7: return appliedInterfaceSettings_.objectInfoRadius ? 1 : 0;
                default: return 0;
            }
        }
        return 0;
    }

    bool PauseMenuController::setOptionIndexForRow(const int row, const int targetIndex, GLFWwindow* window) {
        if (isControlPage()) {
            return false;
        }
        const int optionCount = optionCountForRow(row);
        if (optionCount <= 0) {
            return false;
        }
        if (isSettingRowDisabled(row)) {
            statusMessage_ = disabledReasonForRow(row);
            return false;
        }

        setSelectedSettingRow(std::clamp(row, 0, settingCount() - 1));
        int current = optionIndexForRow(selectedSettingRow_);
        const int target = std::clamp(targetIndex, 0, optionCount - 1);
        while (current != target) {
            const int previous = current;
            adjustSelectedSetting(target > current ? 1 : -1, window);
            current = optionIndexForRow(selectedSettingRow_);
            if (current == previous) {
                break;
            }
        }
        return current == target;
    }

    void PauseMenuController::adjustSelectedSetting(const int delta, GLFWwindow* window) {
        if (isControlPage()) {
            return;
        }

        if (settingsPage_ == SettingsPage::Display) {
            if (selectedSettingRow_ == 1) {
                const WindowMode effectiveMode = pendingDisplayChanges_
                    ? draftDisplaySettings_.windowMode
                    : appliedDisplaySettings_.windowMode;
                if (effectiveMode == WindowMode::Borderless) {
                    statusMessage_ = "RESOLUTION LOCKED";
                    return;
                }
            }

            if (!pendingDisplayChanges_) {
                draftDisplaySettings_ = appliedDisplaySettings_;
            }
            pendingDisplayChanges_ = true;

            auto& settings = draftDisplaySettings_;
            switch (selectedSettingRow_) {
                case 0:
                    settings.windowMode = static_cast<WindowMode>(
                        wrapSelectionRow(static_cast<int>(settings.windowMode) + delta, 3));
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
            pendingDisplayChanges_ = hasPendingDisplayChanges();
            if (!pendingDisplayChanges_) {
                draftDisplaySettings_ = appliedDisplaySettings_;
            }
            statusMessage_.clear();
            return;
        }

        ensureDraftForSelectedRow();

        if (settingsPage_ == SettingsPage::Simulation) {
            auto& settings = draftSimulationSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kMinSpeedChoices, settings.minSimSpeed) + delta,
                        0,
                        static_cast<int>(kMinSpeedChoices.size()) - 1);
                    settings.minSimSpeed = kMinSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.maxSimSpeed < settings.minSimSpeed) {
                        settings.maxSimSpeed = settings.minSimSpeed;
                    }
                    break;
                }
                case 1: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kMaxSpeedChoices, settings.maxSimSpeed) + delta,
                        0,
                        static_cast<int>(kMaxSpeedChoices.size()) - 1);
                    settings.maxSimSpeed = kMaxSpeedChoices[static_cast<std::size_t>(idx)];
                    if (settings.minSimSpeed > settings.maxSimSpeed) {
                        settings.minSimSpeed = settings.maxSimSpeed;
                    }
                    break;
                }
                case 2:
                    settings.gravityEnabled = !settings.gravityEnabled;
                    break;
                case 3: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kGravityStrengthChoices, settings.gravityStrength) + delta,
                        0,
                        static_cast<int>(kGravityStrengthChoices.size()) - 1);
                    settings.gravityStrength = kGravityStrengthChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 4: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kCollisionIterationChoices, settings.collisionIterations) + delta,
                        0,
                        static_cast<int>(kCollisionIterationChoices.size()) - 1);
                    settings.collisionIterations = kCollisionIterationChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                default:
                    break;
            }
        } else if (settingsPage_ == SettingsPage::Camera) {
            auto& settings = draftCameraSettings_;
            switch (selectedSettingRow_) {
                case 0: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kLookSensitivityChoices, settings.lookSensitivity) + delta,
                        0,
                        static_cast<int>(kLookSensitivityChoices.size()) - 1);
                    settings.lookSensitivity = kLookSensitivityChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 1: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kBaseMoveSpeedChoices, settings.baseMoveSpeed) + delta,
                        0,
                        static_cast<int>(kBaseMoveSpeedChoices.size()) - 1);
                    settings.baseMoveSpeed = kBaseMoveSpeedChoices[static_cast<std::size_t>(idx)];
                    break;
                }
                case 2:
                    settings.invertY = !settings.invertY;
                    break;
                case 3: {
                    const int idx = std::clamp(
                        closestChoiceIndex(kFovChoices, settings.fovDegrees) + delta,
                        0,
                        static_cast<int>(kFovChoices.size()) - 1);
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
                    settings.uiScaleIndex = std::clamp(
                        settings.uiScaleIndex + delta,
                        0,
                        static_cast<int>(kUiScaleChoices.size()) - 1);
                    break;
                case 1:
                    settings.showHud = !settings.showHud;
                    break;
                case 2:
                    settings.showCrosshair = !settings.showCrosshair;
                    break;
                case 3:
                    settings.objectInfo = !settings.objectInfo;
                    break;
                case 4:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMaterial = !settings.objectInfoMaterial;
                    break;
                case 5:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoVelocity = !settings.objectInfoVelocity;
                    break;
                case 6:
                    if (!settings.objectInfo) {
                        statusMessage_ = "ENABLE OBJECT INFO FIRST";
                        return;
                    }
                    settings.objectInfoMass = !settings.objectInfoMass;
                    break;
                case 7:
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

        if (settingsPage_ == SettingsPage::Simulation) {
            appliedSimulationSettings_ = draftSimulationSettings_;
        } else if (settingsPage_ == SettingsPage::Camera) {
            appliedCameraSettings_ = draftCameraSettings_;
        } else if (settingsPage_ == SettingsPage::Interface) {
            appliedInterfaceSettings_ = draftInterfaceSettings_;
        }

        markAppliedSelection();
        pendingSelectionEdit_ = false;
        draftSelectionRow_ = -1;
        draftSettingsPage_ = settingsPage_;
        statusMessage_.clear();
        saveSettings();
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
                    FullscreenModeChoice desktopMode{};
                    if (getCachedDesktopMode(monitor, desktopMode)) {
                        targetW = desktopMode.width;
                        targetH = desktopMode.height;
                    } else {
                        targetW = monitorMode->width;
                        targetH = monitorMode->height;
                    }
                    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
                    glfwSetWindowMonitor(
                        window,
                        nullptr,
                        monitorX,
                        monitorY,
                        targetW,
                        targetH,
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

    void PauseMenuController::resetAllSettings(GLFWwindow* window) {
        appliedDisplaySettings_ = DisplaySettings{};
        appliedSimulationSettings_ = SimulationSettings{};
        appliedCameraSettings_ = CameraSettings{};
        appliedInterfaceSettings_ = InterfaceSettings{};
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedDisplaySettings_);
        discardPendingEdit();
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();
    }

    bool PauseMenuController::hasPendingDisplayChanges() const {
        return draftDisplaySettings_.windowMode != appliedDisplaySettings_.windowMode ||
               draftDisplaySettings_.vsync != appliedDisplaySettings_.vsync ||
               draftDisplaySettings_.resolutionIndex != appliedDisplaySettings_.resolutionIndex;
    }

    void PauseMenuController::normalizeAppliedSettings() {
        appliedDisplaySettings_.resolutionIndex =
            std::clamp(appliedDisplaySettings_.resolutionIndex, 0, resolutionChoiceCount() - 1);

        appliedSimulationSettings_.minSimSpeed =
            kMinSpeedChoices[static_cast<std::size_t>(
                closestChoiceIndex(kMinSpeedChoices, appliedSimulationSettings_.minSimSpeed))];
        appliedSimulationSettings_.maxSimSpeed =
            kMaxSpeedChoices[static_cast<std::size_t>(
                closestChoiceIndex(kMaxSpeedChoices, appliedSimulationSettings_.maxSimSpeed))];
        appliedSimulationSettings_.gravityStrength =
            kGravityStrengthChoices[static_cast<std::size_t>(
                closestChoiceIndex(kGravityStrengthChoices, appliedSimulationSettings_.gravityStrength))];
        appliedSimulationSettings_.collisionIterations =
            kCollisionIterationChoices[static_cast<std::size_t>(
                closestChoiceIndex(kCollisionIterationChoices, appliedSimulationSettings_.collisionIterations))];
        if (appliedSimulationSettings_.maxSimSpeed < appliedSimulationSettings_.minSimSpeed) {
            appliedSimulationSettings_.maxSimSpeed = appliedSimulationSettings_.minSimSpeed;
        }

        appliedCameraSettings_.lookSensitivity =
            kLookSensitivityChoices[static_cast<std::size_t>(
                closestChoiceIndex(kLookSensitivityChoices, appliedCameraSettings_.lookSensitivity))];
        appliedCameraSettings_.baseMoveSpeed =
            kBaseMoveSpeedChoices[static_cast<std::size_t>(
                closestChoiceIndex(kBaseMoveSpeedChoices, appliedCameraSettings_.baseMoveSpeed))];
        appliedCameraSettings_.fovDegrees =
            kFovChoices[static_cast<std::size_t>(
                closestChoiceIndex(kFovChoices, appliedCameraSettings_.fovDegrees))];

        appliedInterfaceSettings_.uiScaleIndex = std::clamp(
            appliedInterfaceSettings_.uiScaleIndex,
            0,
            static_cast<int>(kUiScaleChoices.size()) - 1);
    }

    void PauseMenuController::saveSettings() const {
        if (settingsConfigPath_.empty()) {
            return;
        }

        std::ofstream out(settingsConfigPath_, std::ios::trunc);
        if (!out) {
            return;
        }

        out << "WINDOW_MODE=" << windowModeLabel(appliedDisplaySettings_.windowMode) << "\n";
        out << "VSYNC=" << (appliedDisplaySettings_.vsync ? "ON" : "OFF") << "\n";
        out << "RESOLUTION_INDEX=" << appliedDisplaySettings_.resolutionIndex << "\n";
        out << "MIN_SIM_SPEED=" << appliedSimulationSettings_.minSimSpeed << "\n";
        out << "MAX_SIM_SPEED=" << appliedSimulationSettings_.maxSimSpeed << "\n";
        out << "GRAVITY_ENABLED=" << (appliedSimulationSettings_.gravityEnabled ? "ON" : "OFF") << "\n";
        out << "GRAVITY_STRENGTH=" << appliedSimulationSettings_.gravityStrength << "\n";
        out << "COLLISION_ITERATIONS=" << appliedSimulationSettings_.collisionIterations << "\n";
        out << "LOOK_SENSITIVITY=" << appliedCameraSettings_.lookSensitivity << "\n";
        out << "BASE_MOVE_SPEED=" << appliedCameraSettings_.baseMoveSpeed << "\n";
        out << "INVERT_Y=" << (appliedCameraSettings_.invertY ? "ON" : "OFF") << "\n";
        out << "FOV_DEGREES=" << appliedCameraSettings_.fovDegrees << "\n";
        out << "UI_SCALE_INDEX=" << appliedInterfaceSettings_.uiScaleIndex << "\n";
        out << "SHOW_HUD=" << (appliedInterfaceSettings_.showHud ? "ON" : "OFF") << "\n";
        out << "SHOW_CROSSHAIR=" << (appliedInterfaceSettings_.showCrosshair ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO=" << (appliedInterfaceSettings_.objectInfo ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_MATERIAL=" << (appliedInterfaceSettings_.objectInfoMaterial ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_VELOCITY=" << (appliedInterfaceSettings_.objectInfoVelocity ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_MASS=" << (appliedInterfaceSettings_.objectInfoMass ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_RADIUS=" << (appliedInterfaceSettings_.objectInfoRadius ? "ON" : "OFF") << "\n";
    }

    void PauseMenuController::markAppliedSelection() const {
        lastAppliedPage_ = settingsPage_;
        lastAppliedRow_ = selectedSettingRow_;
        applyIndicatorFrames_ = 120;
    }
} // namespace ui

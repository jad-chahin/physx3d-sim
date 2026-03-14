#include "ui/PauseMenuController.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>

namespace ui {
    namespace {
        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;

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
    } // namespace

    void PauseMenuController::applyCurrentDisplaySettings(GLFWwindow* window) {
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedDisplaySettings_);
        discardPendingEdit();
        saveSettings();
    }

    void PauseMenuController::applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings) {
        if (window == nullptr) {
            return;
        }

        if (glfwGetWindowMonitor(window) == nullptr &&
            glfwGetWindowAttrib(window, GLFW_DECORATED) == GLFW_TRUE)
        {
            glfwGetWindowPos(window, &settings.windowedX, &settings.windowedY);
            glfwGetWindowSize(window, &settings.windowedWidth, &settings.windowedHeight);
        }

        GLFWmonitor* monitor = monitorForWindow(window);
        const GLFWvidmode* monitorMode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        int monitorX = 0;
        int monitorY = 0;
        if (monitor != nullptr) {
            glfwGetMonitorPos(monitor, &monitorX, &monitorY);
        }

        if (settings.windowMode == WindowMode::Borderless) {
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
        } else {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);

            int workX = monitorX;
            int workY = monitorY;
            int workW = monitorMode != nullptr ? monitorMode->width : settings.windowedWidth;
            int workH = monitorMode != nullptr ? monitorMode->height : settings.windowedHeight;
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
            const int useW = std::clamp(settings.windowedWidth, kMinWindowWidth, maxClientW);
            const int useH = std::clamp(settings.windowedHeight, kMinWindowHeight, maxClientH);
            const int posX = std::clamp(
                settings.windowedX,
                workX + frameL,
                std::max(workX + frameL, workX + workW - frameR - useW));
            const int posY = std::clamp(
                settings.windowedY,
                workY + frameT,
                std::max(workY + frameT, workY + workH - frameB - useH));

            glfwSetWindowMonitor(window, nullptr, posX, posY, useW, useH, GLFW_DONT_CARE);
            settings.windowedX = posX;
            settings.windowedY = posY;
            settings.windowedWidth = useW;
            settings.windowedHeight = useH;
        }

        glfwSwapInterval(settings.vsync ? 1 : 0);
    }
} // namespace ui

#include "ui/PauseMenuInternal.h"

namespace ui {
using namespace pause_menu_detail;

float PauseMenu::uiScale() const
{
    const int index = std::clamp(applied_.interface.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
    return kUiScaleChoices[static_cast<std::size_t>(index)];
}

void PauseMenu::normalizeSettings()
{
    snapChoice(kMinSpeedChoices, applied_.simulation.minSimSpeed);
    snapChoice(kMaxSpeedChoices, applied_.simulation.maxSimSpeed);
    snapChoice(kGravityStrengthChoices, applied_.simulation.gravityStrength);
    snapChoice(kCollisionIterationChoices, applied_.simulation.velocityIterations);
    snapChoice(kCollisionIterationChoices, applied_.simulation.positionIterations);
    snapChoice(kGlobalRestitutionChoices, applied_.simulation.globalRestitution);
    snapChoice(kLookSensitivityChoices, applied_.camera.lookSensitivity);
    snapChoice(kBaseMoveSpeedChoices, applied_.camera.baseMoveSpeed);
    snapChoice(kFovChoices, applied_.camera.fovDegrees);
    applied_.interface.uiScaleIndex =
        std::clamp(applied_.interface.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
    applied_.interface.minimapZoomIndex =
        std::clamp(applied_.interface.minimapZoomIndex, 0, static_cast<int>(kMinimapZoomChoices.size()) - 1);
    applied_.interface.pathLengthIndex =
        std::clamp(applied_.interface.pathLengthIndex, 0, static_cast<int>(kPathLengthChoices.size()) - 1);
    applied_.interface.pathColorIndex =
        std::clamp(applied_.interface.pathColorIndex, 0, static_cast<int>(kPathColorChoices.size()) - 1);
    applied_.interface.backdropPresetIndex =
        std::clamp(applied_.interface.backdropPresetIndex, 0, static_cast<int>(kBackdropPresetChoices.size()) - 1);
    applyObjectInfoDetailLevel(applied_.interface, objectInfoDetailIndex(applied_.interface));
    if (applied_.simulation.maxSimSpeed < applied_.simulation.minSimSpeed) {
        applied_.simulation.maxSimSpeed = applied_.simulation.minSimSpeed;
    }
    draft_ = applied_;
}

void PauseMenu::loadSettings(const std::string& path)
{
    settingsPath_ = path;

    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = toUpperCopy(trimCopy(std::string_view{line}.substr(0, eq)));
        const std::string value = trimCopy(std::string_view{line}.substr(eq + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        if (key == "WINDOW_MODE") {
            const std::string mode = toUpperCopy(value);
            if (mode == "WINDOWED") {
                applied_.display.windowMode = WindowMode::Windowed;
            }
            if (mode == "BORDERLESS") {
                applied_.display.windowMode = WindowMode::Borderless;
            }
        } else if (key == "VSYNC") {
            parseBool(value, applied_.display.vsync);
        } else if (key == "WINDOWED_X") {
            parseInt(value, applied_.display.windowedX);
        } else if (key == "WINDOWED_Y") {
            parseInt(value, applied_.display.windowedY);
        } else if (key == "WINDOWED_WIDTH") {
            parseInt(value, applied_.display.windowedWidth);
        } else if (key == "WINDOWED_HEIGHT") {
            parseInt(value, applied_.display.windowedHeight);
        } else if (key == "MIN_SIM_SPEED") {
            parseDouble(value, applied_.simulation.minSimSpeed);
        } else if (key == "MAX_SIM_SPEED") {
            parseDouble(value, applied_.simulation.maxSimSpeed);
        } else if (key == "GRAVITY_ENABLED") {
            parseBool(value, applied_.simulation.gravityEnabled);
        } else if (key == "GRAVITY_STRENGTH") {
            parseDouble(value, applied_.simulation.gravityStrength);
        } else if (key == "COLLISIONS_ENABLED") {
            parseBool(value, applied_.simulation.collisionsEnabled);
        } else if (key == "VELOCITY_ITERATIONS" || key == "COLLISION_ITERATIONS") {
            parseInt(value, applied_.simulation.velocityIterations);
        } else if (key == "POSITION_ITERATIONS") {
            parseInt(value, applied_.simulation.positionIterations);
        } else if (key == "GLOBAL_RESTITUTION") {
            parseDouble(value, applied_.simulation.globalRestitution);
        } else if (key == "LOOK_SENSITIVITY") {
            parseFloat(value, applied_.camera.lookSensitivity);
        } else if (key == "BASE_MOVE_SPEED") {
            parseFloat(value, applied_.camera.baseMoveSpeed);
        } else if (key == "INVERT_Y") {
            parseBool(value, applied_.camera.invertY);
        } else if (key == "FOV_DEGREES") {
            parseFloat(value, applied_.camera.fovDegrees);
        } else if (key == "UI_SCALE_INDEX") {
            parseInt(value, applied_.interface.uiScaleIndex);
        } else if (key == "MINIMAP_ZOOM_INDEX") {
            parseInt(value, applied_.interface.minimapZoomIndex);
        } else if (key == "MINIMAP_RANGE") {
            float minimapRange = 0.0f;
            if (parseFloat(value, minimapRange)) {
                applied_.interface.minimapZoomIndex = closestChoiceIndex(kMinimapZoomChoices, minimapRange);
            }
        } else if (key == "PATH_LENGTH_INDEX") {
            parseInt(value, applied_.interface.pathLengthIndex);
        } else if (key == "PATH_LENGTH") {
            int pathLength = 0;
            if (parseInt(value, pathLength)) {
                applied_.interface.pathLengthIndex = closestChoiceIndex(kPathLengthChoices, pathLength);
            }
        } else if (key == "PATH_COLOR_INDEX") {
            parseInt(value, applied_.interface.pathColorIndex);
        } else if (key == "PATH_COLOR") {
            const std::string colorName = toUpperCopy(value);
            for (std::size_t i = 0; i < kPathColorChoices.size(); ++i) {
                if (colorName == kPathColorChoices[i]) {
                    applied_.interface.pathColorIndex = static_cast<int>(i);
                    break;
                }
            }
        } else if (key == "BACKDROP_PRESET_INDEX") {
            parseInt(value, applied_.interface.backdropPresetIndex);
        } else if (key == "BACKDROP_PRESET") {
            const std::string presetName = toUpperCopy(value);
            for (std::size_t i = 0; i < kBackdropPresetChoices.size(); ++i) {
                if (presetName == kBackdropPresetChoices[i]) {
                    applied_.interface.backdropPresetIndex = static_cast<int>(i);
                    break;
                }
            }
        } else if (key == "SHOW_SIM_SPEED") {
            parseBool(value, applied_.interface.showSimulationSpeed);
        } else if (key == "SHOW_FPS") {
            parseBool(value, applied_.interface.showFps);
        } else if (key == "SHOW_ELAPSED_TIME") {
            parseBool(value, applied_.interface.showElapsedTime);
        } else if (key == "SHOW_MINIMAP") {
            parseBool(value, applied_.interface.showMinimap);
        } else if (key == "SHOW_COORDINATES") {
            parseBool(value, applied_.interface.showCoordinates);
        } else if (key == "SHOW_CROSSHAIR") {
            parseBool(value, applied_.interface.showCrosshair);
        } else if (key == "DRAW_PATH") {
            parseBool(value, applied_.interface.drawPath);
        } else if (key == "OBJECT_INFO_DETAIL_INDEX") {
            int detailIndex = objectInfoDetailIndex(applied_.interface);
            if (parseInt(value, detailIndex)) {
                applyObjectInfoDetailLevel(applied_.interface, detailIndex);
            }
        } else if (key == "OBJECT_INFO_DETAIL_LEVEL") {
            int detailIndex = objectInfoDetailIndex(applied_.interface);
            if (parseInt(value, detailIndex)) {
                applyObjectInfoDetailLevel(applied_.interface, detailIndex);
            }
        } else if (key == "OBJECT_INFO") {
            parseBool(value, applied_.interface.objectInfo);
        } else if (key == "OBJECT_INFO_MATERIAL") {
            parseBool(value, applied_.interface.objectInfoMaterial);
        } else if (key == "OBJECT_INFO_VELOCITY") {
            parseBool(value, applied_.interface.objectInfoVelocity);
        } else if (key == "OBJECT_INFO_MASS") {
            parseBool(value, applied_.interface.objectInfoMass);
        } else if (key == "OBJECT_INFO_RADIUS") {
            parseBool(value, applied_.interface.objectInfoRadius);
        } else if (key == "OBJECT_INFO_ANGULAR_SPEED") {
            parseBool(value, applied_.interface.objectInfoAngularSpeed);
        } else if (key == "OBJECT_INFO_BODY_TYPE") {
            parseBool(value, applied_.interface.objectInfoBodyType);
        }
    }

    normalizeSettings();
    saveSettings();
}

void PauseMenu::saveSettings() const
{
    if (settingsPath_.empty()) {
        return;
    }

    std::ofstream out(settingsPath_, std::ios::trunc);
    if (!out) {
        return;
    }

    out << "WINDOW_MODE=" << (applied_.display.windowMode == WindowMode::Windowed ? "WINDOWED" : "BORDERLESS") << '\n';
    out << "VSYNC=" << formatToggle(applied_.display.vsync) << '\n';
    out << "WINDOWED_X=" << applied_.display.windowedX << '\n';
    out << "WINDOWED_Y=" << applied_.display.windowedY << '\n';
    out << "WINDOWED_WIDTH=" << applied_.display.windowedWidth << '\n';
    out << "WINDOWED_HEIGHT=" << applied_.display.windowedHeight << '\n';
    out << "MIN_SIM_SPEED=" << applied_.simulation.minSimSpeed << '\n';
    out << "MAX_SIM_SPEED=" << applied_.simulation.maxSimSpeed << '\n';
    out << "GRAVITY_ENABLED=" << formatToggle(applied_.simulation.gravityEnabled) << '\n';
    out << "GRAVITY_STRENGTH=" << applied_.simulation.gravityStrength << '\n';
    out << "COLLISIONS_ENABLED=" << formatToggle(applied_.simulation.collisionsEnabled) << '\n';
    out << "VELOCITY_ITERATIONS=" << applied_.simulation.velocityIterations << '\n';
    out << "POSITION_ITERATIONS=" << applied_.simulation.positionIterations << '\n';
    out << "GLOBAL_RESTITUTION=" << applied_.simulation.globalRestitution << '\n';
    out << "LOOK_SENSITIVITY=" << applied_.camera.lookSensitivity << '\n';
    out << "BASE_MOVE_SPEED=" << applied_.camera.baseMoveSpeed << '\n';
    out << "INVERT_Y=" << formatToggle(applied_.camera.invertY) << '\n';
    out << "FOV_DEGREES=" << applied_.camera.fovDegrees << '\n';
    out << "UI_SCALE_INDEX=" << applied_.interface.uiScaleIndex << '\n';
    out << "MINIMAP_ZOOM_INDEX=" << applied_.interface.minimapZoomIndex << '\n';
    out << "PATH_LENGTH_INDEX=" << applied_.interface.pathLengthIndex << '\n';
    out << "PATH_COLOR_INDEX=" << applied_.interface.pathColorIndex << '\n';
    out << "BACKDROP_PRESET_INDEX=" << applied_.interface.backdropPresetIndex << '\n';
    out << "BACKDROP_PRESET="
        << kBackdropPresetChoices[static_cast<std::size_t>(applied_.interface.backdropPresetIndex)]
        << '\n';
    out << "SHOW_SIM_SPEED=" << formatToggle(applied_.interface.showSimulationSpeed) << '\n';
    out << "SHOW_FPS=" << formatToggle(applied_.interface.showFps) << '\n';
    out << "SHOW_ELAPSED_TIME=" << formatToggle(applied_.interface.showElapsedTime) << '\n';
    out << "SHOW_MINIMAP=" << formatToggle(applied_.interface.showMinimap) << '\n';
    out << "SHOW_COORDINATES=" << formatToggle(applied_.interface.showCoordinates) << '\n';
    out << "SHOW_CROSSHAIR=" << formatToggle(applied_.interface.showCrosshair) << '\n';
    out << "DRAW_PATH=" << formatToggle(applied_.interface.drawPath) << '\n';
    out << "OBJECT_INFO=" << formatToggle(applied_.interface.objectInfo) << '\n';
    out << "OBJECT_INFO_DETAIL_INDEX=" << objectInfoDetailIndex(applied_.interface) << '\n';
}

void PauseMenu::applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings)
{
    if (window == nullptr) {
        return;
    }

    if (glfwGetWindowMonitor(window) == nullptr &&
        glfwGetWindowAttrib(window, GLFW_DECORATED) == GLFW_TRUE)
    {
        glfwGetWindowPos(window, &settings.windowedX, &settings.windowedY);
        glfwGetWindowSize(window, &settings.windowedWidth, &settings.windowedHeight);
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
    int monitorX = 0;
    int monitorY = 0;
    if (monitor != nullptr) {
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);
    }

    if (settings.windowMode == WindowMode::Borderless) {
        if (mode != nullptr) {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
            glfwSetWindowMonitor(window, nullptr, monitorX, monitorY, mode->width, mode->height, GLFW_DONT_CARE);
        }
    } else {
        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
        settings.windowedWidth = std::max(kMinWindowWidth, settings.windowedWidth);
        settings.windowedHeight = std::max(kMinWindowHeight, settings.windowedHeight);
        glfwSetWindowMonitor(
            window,
            nullptr,
            settings.windowedX,
            settings.windowedY,
            settings.windowedWidth,
            settings.windowedHeight,
            GLFW_DONT_CARE);
    }

    glfwSwapInterval(settings.vsync ? 1 : 0);
}

void PauseMenu::applyCurrentDisplaySettings(GLFWwindow* window)
{
    normalizeSettings();
    applyDisplaySettings(window, applied_.display);
    draft_ = applied_;
    saveSettings();
}

} // namespace ui

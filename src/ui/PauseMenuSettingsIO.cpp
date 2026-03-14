#include "ui/PauseMenuController.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace ui {
    namespace {
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
            return false;
        }
    } // namespace

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
                    WindowMode parsed = WindowMode::Borderless;
                    if (tryParseWindowMode(value, parsed)) {
                        appliedDisplaySettings_.windowMode = parsed;
                    }
                } else if (key == "VSYNC") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedDisplaySettings_.vsync = parsed;
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
                } else if (key == "COLLISIONS_ENABLED") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedSimulationSettings_.collisionsEnabled = parsed;
                    }
                } else if (key == "COLLISION_ITERATIONS") {
                    int parsed = 0;
                    if (tryParseInt(value, parsed)) {
                        appliedSimulationSettings_.collisionIterations = parsed;
                    }
                } else if (key == "JOINT_ITERATIONS") {
                    int parsed = 0;
                    if (tryParseInt(value, parsed)) {
                        appliedSimulationSettings_.jointIterations = parsed;
                    }
                } else if (key == "GLOBAL_RESTITUTION") {
                    double parsed = 0.0;
                    if (tryParseDouble(value, parsed)) {
                        appliedSimulationSettings_.globalRestitution = parsed;
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
                } else if (key == "SHOW_PHYSICS_STATS" || key == "SHOW_DEBUG_STATS") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.showPhysicsStats = parsed;
                    }
                } else if (key == "DRAW_PATH") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.drawPath = parsed;
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
                } else if (key == "OBJECT_INFO_ANGULAR_SPEED") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoAngularSpeed = parsed;
                    }
                } else if (key == "OBJECT_INFO_BODY_TYPE") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoBodyType = parsed;
                    }
                } else if (key == "OBJECT_INFO_JOINT_COUNT") {
                    bool parsed = false;
                    if (tryParseBool(value, parsed)) {
                        appliedInterfaceSettings_.objectInfoJointCount = parsed;
                    }
                }
            }
        }

        normalizeAppliedSettings();
        discardPendingEdit();
        saveSettings();
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
        out << "MIN_SIM_SPEED=" << appliedSimulationSettings_.minSimSpeed << "\n";
        out << "MAX_SIM_SPEED=" << appliedSimulationSettings_.maxSimSpeed << "\n";
        out << "GRAVITY_ENABLED=" << (appliedSimulationSettings_.gravityEnabled ? "ON" : "OFF") << "\n";
        out << "GRAVITY_STRENGTH=" << appliedSimulationSettings_.gravityStrength << "\n";
        out << "COLLISIONS_ENABLED=" << (appliedSimulationSettings_.collisionsEnabled ? "ON" : "OFF") << "\n";
        out << "COLLISION_ITERATIONS=" << appliedSimulationSettings_.collisionIterations << "\n";
        out << "JOINT_ITERATIONS=" << appliedSimulationSettings_.jointIterations << "\n";
        out << "GLOBAL_RESTITUTION=" << appliedSimulationSettings_.globalRestitution << "\n";
        out << "LOOK_SENSITIVITY=" << appliedCameraSettings_.lookSensitivity << "\n";
        out << "BASE_MOVE_SPEED=" << appliedCameraSettings_.baseMoveSpeed << "\n";
        out << "INVERT_Y=" << (appliedCameraSettings_.invertY ? "ON" : "OFF") << "\n";
        out << "FOV_DEGREES=" << appliedCameraSettings_.fovDegrees << "\n";
        out << "UI_SCALE_INDEX=" << appliedInterfaceSettings_.uiScaleIndex << "\n";
        out << "SHOW_HUD=" << (appliedInterfaceSettings_.showHud ? "ON" : "OFF") << "\n";
        out << "SHOW_CROSSHAIR=" << (appliedInterfaceSettings_.showCrosshair ? "ON" : "OFF") << "\n";
        out << "SHOW_DEBUG_STATS=" << (appliedInterfaceSettings_.showPhysicsStats ? "ON" : "OFF") << "\n";
        out << "DRAW_PATH=" << (appliedInterfaceSettings_.drawPath ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO=" << (appliedInterfaceSettings_.objectInfo ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_MATERIAL=" << (appliedInterfaceSettings_.objectInfoMaterial ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_VELOCITY=" << (appliedInterfaceSettings_.objectInfoVelocity ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_MASS=" << (appliedInterfaceSettings_.objectInfoMass ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_RADIUS=" << (appliedInterfaceSettings_.objectInfoRadius ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_ANGULAR_SPEED=" << (appliedInterfaceSettings_.objectInfoAngularSpeed ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_BODY_TYPE=" << (appliedInterfaceSettings_.objectInfoBodyType ? "ON" : "OFF") << "\n";
        out << "OBJECT_INFO_JOINT_COUNT=" << (appliedInterfaceSettings_.objectInfoJointCount ? "ON" : "OFF") << "\n";
    }
} // namespace ui

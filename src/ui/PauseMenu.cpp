#include "ui/PauseMenu.h"
#include "ui/PauseMenuLayout.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <vector>

namespace ui {
namespace {

constexpr std::array<const char*, 5> kPageNames{{
    "DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS",
}};

constexpr std::array<const char*, 2> kWindowModes{{
    "BORDERLESS", "WINDOWED",
}};

template <typename Settings> struct RowDescriptor { const char* label; RowKind kind; std::string (*value)(const Settings&); bool (*boolValue)(const Settings&); void (*adjust)(Settings&, int); };
enum class ActionSection { Top, Bottom };
struct ActionDefinition { ActionKind kind; const char* label; ActionSection section; bool (*visible)(SettingsPage, const input::ControlBindings&); };
struct PopupDefinition { PauseMenu::PopupKind kind; const char* title; const char* body; const char* confirmLabel; const char* cancelLabel; };

template <typename T, std::size_t N>
int closestChoiceIndex(const std::array<T, N>& choices, const T value) {
    int bestIdx = 0;
    auto bestDiff = static_cast<double>(choices[0] - value);
    bestDiff = bestDiff < 0.0 ? -bestDiff : bestDiff;
    for (std::size_t i = 1; i < N; ++i) {
        auto diff = static_cast<double>(choices[i] - value);
        diff = diff < 0.0 ? -diff : diff;
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

template <typename T, std::size_t N> void snapChoice(const std::array<T, N>& choices, T& value) { value = choices[static_cast<std::size_t>(closestChoiceIndex(choices, value))]; }

std::string trimCopy(const std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n"); if (first == std::string_view::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n"); return std::string{text.substr(first, last - first + 1)};
}

std::string toUpperCopy(std::string text) {
    for (char& c : text) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return text;
}

bool parseBool(const std::string_view value, bool& out) {
    const std::string text = toUpperCopy(trimCopy(value));
    if (text == "ON" || text == "TRUE" || text == "1" || text == "YES") return out = true, true;
    if (text == "OFF" || text == "FALSE" || text == "0" || text == "NO") return out = false, true;
    return false;
}

bool parseInt(const std::string_view value, int& out) {
    const std::string text = trimCopy(value); if (text.empty()) return false;
    try {
        std::size_t pos = 0; const int parsed = std::stoi(text, &pos); if (pos != text.size()) return false;
        out = parsed; return true;
    } catch (...) {
        return false;
    }
}

bool parseDouble(const std::string_view value, double& out) {
    const std::string text = trimCopy(value); if (text.empty()) return false;
    try {
        std::size_t pos = 0; const double parsed = std::stod(text, &pos); if (pos != text.size()) return false;
        out = parsed; return true;
    } catch (...) {
        return false;
    }
}

bool parseFloat(const std::string_view value, float& out) { double parsed = 0.0; if (!parseDouble(value, parsed)) return false; out = static_cast<float>(parsed); return true; }

std::string formatToggle(const bool value) { return value ? "ON" : "OFF"; }

std::string formatSpeed(double value) {
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

std::string formatFloat(const float value, const char* suffix = "") {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4g%s", value, suffix);
    return buffer;
}

std::string formatGravityStrength(const double value) {
    const int idx = closestChoiceIndex(kGravityStrengthChoices, value);
    constexpr int kUnityIndex = 9;
    if (idx < kUnityIndex) {
        return formatFloat(static_cast<float>(idx + 1) * 0.1f, "X");
    }
    if (idx == kUnityIndex) {
        return "1X";
    }
    return std::to_string(idx - kUnityIndex + 1) + "X";
}

std::string formatPercent(const double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0, 1.0) * 100.0);
    return buffer;
}

bool isDisabledReasonMessage(const std::string& message) {
    return message == "ENABLE SHOW MINIMAP FIRST" ||
        message == "ENABLE DRAW PATH FIRST" ||
        message == "ENABLE OBJECT INFO FIRST";
}

constexpr int kInterfaceRowUiScale = 0;
constexpr int kInterfaceRowMinimapZoom = 5;
constexpr int kInterfaceRowDetailLevel = 9;
constexpr std::array<const char*, 2> kObjectInfoDetailChoices{{
    "BASIC", "FULL",
}};

int objectInfoDetailIndex(const InterfaceSettings& settings) {
    if (settings.objectInfoMaterial || settings.objectInfoAngularSpeed || settings.objectInfoBodyType) {
        return 1;
    }
    return 0;
}

void applyObjectInfoDetailLevel(InterfaceSettings& settings, const int detailIndex) {
    const int clamped = std::clamp(detailIndex, 0, static_cast<int>(kObjectInfoDetailChoices.size()) - 1);
    settings.objectInfoMaterial = clamped >= 1;
    settings.objectInfoVelocity = true;
    settings.objectInfoMass = true;
    settings.objectInfoRadius = true;
    settings.objectInfoAngularSpeed = clamped >= 1;
    settings.objectInfoBodyType = clamped >= 1;
}

void applyInterfacePageSettings(InterfaceSettings& dst, const InterfaceSettings& src) {
    dst.uiScaleIndex = src.uiScaleIndex;
    dst.minimapZoomIndex = src.minimapZoomIndex;
    dst.showSimulationSpeed = src.showSimulationSpeed;
    dst.showFps = src.showFps;
    dst.showElapsedTime = src.showElapsedTime;
    dst.showMinimap = src.showMinimap;
    dst.showCoordinates = src.showCoordinates;
    dst.showCrosshair = src.showCrosshair;
    dst.objectInfo = src.objectInfo;
    dst.objectInfoMaterial = src.objectInfoMaterial;
    dst.objectInfoVelocity = src.objectInfoVelocity;
    dst.objectInfoMass = src.objectInfoMass;
    dst.objectInfoRadius = src.objectInfoRadius;
    dst.objectInfoAngularSpeed = src.objectInfoAngularSpeed;
    dst.objectInfoBodyType = src.objectInfoBodyType;
}

template <typename T, std::size_t N> void adjustChoice(const std::array<T, N>& choices, T& value, const int delta) { int idx = closestChoiceIndex(choices, value); idx = std::clamp(idx + delta, 0, static_cast<int>(choices.size()) - 1); value = choices[static_cast<std::size_t>(idx)]; }

template <typename Settings, std::size_t N>
void adjustRowFromDescriptors(
    const std::array<RowDescriptor<Settings>, N>& descriptors,
    Settings& settings,
    const std::size_t rowIndex,
    const int delta)
{
    descriptors[rowIndex].adjust(settings, delta);
}

template <typename Settings, std::size_t N, typename DisabledFn>
void appendRowsFromDescriptors(
    MenuView& view,
    const std::array<RowDescriptor<Settings>, N>& descriptors,
    const Settings& settings,
    const int selectedRow,
    const bool rowsFocused,
    DisabledFn&& disabledFn)
{
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        const auto& descriptor = descriptors[i];
        const int row = static_cast<int>(i);
        const bool disabled = disabledFn(row);
        view.rows.push_back(ViewRow{
            .label = descriptor.label,
            .value = descriptor.value(settings),
            .kind = descriptor.kind,
            .disabled = disabled,
            .selected = rowsFocused && row == selectedRow,
            .boolValue = descriptor.boolValue != nullptr ? descriptor.boolValue(settings) : false,
        });
    }
}

constexpr std::array<RowDescriptor<DisplaySettings>, 2> kDisplayRows{{
    {
        "WINDOW MODE",
        RowKind::Choice,
        [](const DisplaySettings& settings) { return std::string{kWindowModes[static_cast<int>(settings.windowMode)]}; },
        nullptr,
        [](DisplaySettings& settings, int) {
            settings.windowMode = settings.windowMode == WindowMode::Borderless ? WindowMode::Windowed : WindowMode::Borderless;
        },
    },
    {
        "VSYNC",
        RowKind::Toggle,
        [](const DisplaySettings& settings) { return formatToggle(settings.vsync); },
        [](const DisplaySettings& settings) { return settings.vsync; },
        [](DisplaySettings& settings, int) { settings.vsync = !settings.vsync; },
    },
}};

constexpr std::array<RowDescriptor<SimulationSettings>, 8> kSimulationRows{{
    {
        "MIN SPEED",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return formatSpeed(settings.minSimSpeed); },
        nullptr,
        [](SimulationSettings& settings, int delta) {
            adjustChoice(kMinSpeedChoices, settings.minSimSpeed, delta);
            if (settings.maxSimSpeed < settings.minSimSpeed) {
                settings.maxSimSpeed = settings.minSimSpeed;
            }
        },
    },
    {
        "MAX SPEED",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return formatSpeed(settings.maxSimSpeed); },
        nullptr,
        [](SimulationSettings& settings, int delta) {
            adjustChoice(kMaxSpeedChoices, settings.maxSimSpeed, delta);
            if (settings.minSimSpeed > settings.maxSimSpeed) {
                settings.minSimSpeed = settings.maxSimSpeed;
            }
        },
    },
    {
        "GRAVITY",
        RowKind::Toggle,
        [](const SimulationSettings& settings) { return formatToggle(settings.gravityEnabled); },
        [](const SimulationSettings& settings) { return settings.gravityEnabled; },
        [](SimulationSettings& settings, int) { settings.gravityEnabled = !settings.gravityEnabled; },
    },
    {
        "GRAVITY STRENGTH",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return formatGravityStrength(settings.gravityStrength); },
        nullptr,
        [](SimulationSettings& settings, int delta) { adjustChoice(kGravityStrengthChoices, settings.gravityStrength, delta); },
    },
    {
        "COLLISIONS",
        RowKind::Toggle,
        [](const SimulationSettings& settings) { return formatToggle(settings.collisionsEnabled); },
        [](const SimulationSettings& settings) { return settings.collisionsEnabled; },
        [](SimulationSettings& settings, int) { settings.collisionsEnabled = !settings.collisionsEnabled; },
    },
    {
        "VELOCITY ITERATIONS",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return std::to_string(settings.velocityIterations); },
        nullptr,
        [](SimulationSettings& settings, int delta) { adjustChoice(kCollisionIterationChoices, settings.velocityIterations, delta); },
    },
    {
        "POSITION ITERATIONS",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return std::to_string(settings.positionIterations); },
        nullptr,
        [](SimulationSettings& settings, int delta) { adjustChoice(kCollisionIterationChoices, settings.positionIterations, delta); },
    },
    {
        "GLOBAL BOUNCE",
        RowKind::Choice,
        [](const SimulationSettings& settings) { return formatPercent(settings.globalRestitution); },
        nullptr,
        [](SimulationSettings& settings, int delta) { adjustChoice(kGlobalRestitutionChoices, settings.globalRestitution, delta); },
    },
}};

constexpr std::array<RowDescriptor<CameraSettings>, 4> kCameraRows{{
    {
        "LOOK SENSITIVITY",
        RowKind::Choice,
        [](const CameraSettings& settings) { return formatFloat(settings.lookSensitivity); },
        nullptr,
        [](CameraSettings& settings, int delta) { adjustChoice(kLookSensitivityChoices, settings.lookSensitivity, delta); },
    },
    {
        "MOVE SPEED",
        RowKind::Choice,
        [](const CameraSettings& settings) { return formatFloat(settings.baseMoveSpeed); },
        nullptr,
        [](CameraSettings& settings, int delta) { adjustChoice(kBaseMoveSpeedChoices, settings.baseMoveSpeed, delta); },
    },
    {
        "INVERT Y",
        RowKind::Toggle,
        [](const CameraSettings& settings) { return formatToggle(settings.invertY); },
        [](const CameraSettings& settings) { return settings.invertY; },
        [](CameraSettings& settings, int) { settings.invertY = !settings.invertY; },
    },
    {
        "FOV",
        RowKind::Choice,
        [](const CameraSettings& settings) { return formatFloat(settings.fovDegrees, " DEG"); },
        nullptr,
        [](CameraSettings& settings, int delta) { adjustChoice(kFovChoices, settings.fovDegrees, delta); },
    },
}};

constexpr std::array<RowDescriptor<InterfaceSettings>, 10> kInterfaceRows{{
    {
        "UI SCALE",
        RowKind::Choice,
        [](const InterfaceSettings& settings) { return formatFloat(kUiScaleChoices[static_cast<std::size_t>(settings.uiScaleIndex)], "X"); },
        nullptr,
        [](InterfaceSettings& settings, int delta) {
            settings.uiScaleIndex = std::clamp(settings.uiScaleIndex + delta, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
        },
    },
    {"SHOW SPEED", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showSimulationSpeed); }, [](const InterfaceSettings& s) { return s.showSimulationSpeed; }, [](InterfaceSettings& s, int) { s.showSimulationSpeed = !s.showSimulationSpeed; }},
    {"SHOW FPS", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showFps); }, [](const InterfaceSettings& s) { return s.showFps; }, [](InterfaceSettings& s, int) { s.showFps = !s.showFps; }},
    {"SHOW TIME", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showElapsedTime); }, [](const InterfaceSettings& s) { return s.showElapsedTime; }, [](InterfaceSettings& s, int) { s.showElapsedTime = !s.showElapsedTime; }},
    {"SHOW MINIMAP", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showMinimap); }, [](const InterfaceSettings& s) { return s.showMinimap; }, [](InterfaceSettings& s, int) { s.showMinimap = !s.showMinimap; }},
    {
        "MINIMAP ZOOM",
        RowKind::Choice,
        [](const InterfaceSettings& settings) {
            return formatFloat(kMinimapZoomChoices[static_cast<std::size_t>(settings.minimapZoomIndex)], "M");
        },
        nullptr,
        [](InterfaceSettings& settings, int delta) {
            settings.minimapZoomIndex =
                std::clamp(settings.minimapZoomIndex + delta, 0, static_cast<int>(kMinimapZoomChoices.size()) - 1);
        },
    },
    {"SHOW COORDS", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showCoordinates); }, [](const InterfaceSettings& s) { return s.showCoordinates; }, [](InterfaceSettings& s, int) { s.showCoordinates = !s.showCoordinates; }},
    {"CROSSHAIR", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.showCrosshair); }, [](const InterfaceSettings& s) { return s.showCrosshair; }, [](InterfaceSettings& s, int) { s.showCrosshair = !s.showCrosshair; }},
    {"OBJECT INFO", RowKind::Toggle, [](const InterfaceSettings& s) { return formatToggle(s.objectInfo); }, [](const InterfaceSettings& s) { return s.objectInfo; }, [](InterfaceSettings& s, int) { s.objectInfo = !s.objectInfo; }},
    {
        "DETAIL LEVEL",
        RowKind::Choice,
        [](const InterfaceSettings& settings) {
            return std::string{kObjectInfoDetailChoices[static_cast<std::size_t>(objectInfoDetailIndex(settings))]};
        },
        nullptr,
        [](InterfaceSettings& settings, int delta) {
            applyObjectInfoDetailLevel(settings, objectInfoDetailIndex(settings) + delta);
        },
    },
}};

bool actionAlwaysVisible(SettingsPage, const input::ControlBindings&) { return true; }
bool actionResetWorldVisible(const SettingsPage page, const input::ControlBindings&) { return page == SettingsPage::Simulation; }
bool actionResetControlsVisible(const SettingsPage page, const input::ControlBindings& controls) { return page == SettingsPage::Controls && controls != input::ControlBindings{}; }

constexpr std::array<ActionDefinition, 5> kActionDefinitions{{
    {ActionKind::ResetSettings, "", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::Close, "X", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::Exit, "EXIT TO HOME", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::ResetWorld, "RESET WORLD", ActionSection::Bottom, actionResetWorldVisible},
    {ActionKind::ResetControls, "RESET CONTROLS", ActionSection::Bottom, actionResetControlsVisible},
}};

constexpr std::array<PopupDefinition, 4> kPopupDefinitions{{
    {PauseMenu::PopupKind::ResetSettings, "ARE YOU SURE?", "RESET ALL SETTINGS", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ResetWorld, "ARE YOU SURE?", "RESET WORLD", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ResetControls, "ARE YOU SURE?", "RESET CONTROLS TO DEFAULTS", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ExitToHome, "ARE YOU SURE?", "EXIT TO HOME", "EXIT", "CANCEL"},
}};

std::vector<const ActionDefinition*> visibleActionsForSection(
    const SettingsPage page,
    const ActionSection section,
    const input::ControlBindings& controls)
{
    std::vector<const ActionDefinition*> actions;
    for (const auto& definition : kActionDefinitions) {
        if (definition.section == section && definition.visible(page, controls)) {
            actions.push_back(&definition);
        }
    }
    return actions;
}

const PopupDefinition* popupDefinitionFor(const PauseMenu::PopupKind kind) {
    for (const auto& definition : kPopupDefinitions) {
        if (definition.kind == kind) {
            return &definition;
        }
    }
    return nullptr;
}

ActionKind actionKindFromId(const int id) {
    switch (static_cast<ActionKind>(id)) {
        case ActionKind::ResetWorld:
        case ActionKind::ResetControls:
        case ActionKind::ResetSettings:
        case ActionKind::Close:
        case ActionKind::Exit:
            return static_cast<ActionKind>(id);
    }
    return ActionKind::Close;
}

SettingsPage pageFromNumberKey(const int key) {
    switch (key) {
        case GLFW_KEY_1: return SettingsPage::Display;
        case GLFW_KEY_2: return SettingsPage::Simulation;
        case GLFW_KEY_3: return SettingsPage::Camera;
        case GLFW_KEY_4: return SettingsPage::Interface;
        case GLFW_KEY_5: return SettingsPage::Controls;
        default: return SettingsPage::Display;
    }
}

} // namespace

float PauseMenu::uiScale() const {
    const int index = std::clamp(applied_.interface.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
    return kUiScaleChoices[static_cast<std::size_t>(index)];
}

void PauseMenu::normalizeSettings() {
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
    applyObjectInfoDetailLevel(applied_.interface, objectInfoDetailIndex(applied_.interface));
    if (applied_.simulation.maxSimSpeed < applied_.simulation.minSimSpeed) {
        applied_.simulation.maxSimSpeed = applied_.simulation.minSimSpeed;
    }
    draft_ = applied_;
}

void PauseMenu::loadSettings(const std::string& path) {
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
            if (mode == "WINDOWED") applied_.display.windowMode = WindowMode::Windowed;
            if (mode == "BORDERLESS") applied_.display.windowMode = WindowMode::Borderless;
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

void PauseMenu::saveSettings() const {
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

void PauseMenu::applyDisplaySettings(GLFWwindow* window, DisplaySettings& settings) {
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
        constexpr int kMinWindowWidth = 960, kMinWindowHeight = 540;
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

void PauseMenu::applyCurrentDisplaySettings(GLFWwindow* window) {
    normalizeSettings();
    applyDisplaySettings(window, applied_.display);
    draft_ = applied_;
    saveSettings();
}

int PauseMenu::rowCount() const {
    switch (page_) {
        case SettingsPage::Display: return static_cast<int>(kDisplayRows.size());
        case SettingsPage::Simulation: return static_cast<int>(kSimulationRows.size()) + 3;
        case SettingsPage::Camera: return static_cast<int>(kCameraRows.size());
        case SettingsPage::Interface: return static_cast<int>(kInterfaceRows.size());
        case SettingsPage::Controls: return static_cast<int>(input::rebindActions().size());
    }
    return 0;
}

bool PauseMenu::fieldDisabled(const int row) const {
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

std::string PauseMenu::disabledReason(const int row) const {
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

void PauseMenu::cyclePage(const int delta) {
    const int next = (static_cast<int>(page_) + delta + 5) % 5;
    selectPage(static_cast<SettingsPage>(next));
}

void PauseMenu::selectPage(const SettingsPage page) {
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

void PauseMenu::moveSelectionVertical(const int delta, const input::ControlBindings& controls) {
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

void PauseMenu::openPopup(const PopupKind popup) {
    popup_ = popup;
    focusArea_ = FocusArea::Popup;
    popupConfirmSelected_ = true;
}

void PauseMenu::closePopup() {
    popup_ = PopupKind::None;
    focusArea_ = FocusArea::Rows;
}

bool PauseMenu::beginRebindForSelectedRow() {
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

void PauseMenu::resetAllSettings(GLFWwindow* window) {
    applied_ = SettingsBundle{};
    normalizeSettings();
    applyDisplaySettings(window, applied_.display);
    saveSettings();
    statusMessage_.clear();
}

void PauseMenu::confirmPopup(GLFWwindow* window, input::ControlBindings& controls, const std::string& controlsConfigPath) {
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

void PauseMenu::adjustCurrentPageRow(const int delta) {
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

bool PauseMenu::selectedRowActsAsToggle() const {
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
                selectedRow_ != kInterfaceRowDetailLevel;
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
                selectedRow_ == kInterfaceRowDetailLevel;
        case SettingsPage::Controls:
            return false;
    }
    return false;
}

void PauseMenu::moveSelectionHorizontal(const int delta, GLFWwindow* window) {
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

void PauseMenu::triggerAction(
    const ActionKind action)
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

void PauseMenu::commitCurrentPageSettings(GLFWwindow* window) {
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

void PauseMenu::updateContinuousInput(GLFWwindow* window, const input::ControlBindings& controls) {
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

void PauseMenu::appendCurrentPageRows(MenuView& view, const input::ControlBindings& controls) const {
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

MenuView PauseMenu::buildView(const input::ControlBindings& controls) const {
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

bool PauseMenu::consumeResetWorldRequest() {
    const bool requested = resetWorldRequested_;
    resetWorldRequested_ = false;
    return requested;
}

} // namespace ui

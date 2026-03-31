#ifndef PHYSICS3D_UI_PAUSEMENUINTERNAL_H
#define PHYSICS3D_UI_PAUSEMENUINTERNAL_H

#include "ui/PauseMenu.h"

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

namespace ui::pause_menu_detail {

inline constexpr std::array<const char*, 5> kPageNames{{
    "DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS",
}};

inline constexpr std::array<const char*, 2> kWindowModes{{
    "BORDERLESS", "WINDOWED",
}};

template <typename Settings>
struct RowDescriptor {
    const char* label;
    RowKind kind;
    std::string (*value)(const Settings&);
    bool (*boolValue)(const Settings&);
    void (*adjust)(Settings&, int);
};

enum class ActionSection { Top, Bottom };

struct ActionDefinition {
    ActionKind kind;
    const char* label;
    ActionSection section;
    bool (*visible)(SettingsPage, const input::ControlBindings&);
};

struct PopupDefinition {
    PauseMenu::PopupKind kind;
    const char* title;
    const char* body;
    const char* confirmLabel;
    const char* cancelLabel;
};

template <typename T, std::size_t N>
int closestChoiceIndex(const std::array<T, N>& choices, const T value)
{
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

template <typename T, std::size_t N>
void snapChoice(const std::array<T, N>& choices, T& value)
{
    value = choices[static_cast<std::size_t>(closestChoiceIndex(choices, value))];
}

inline std::string trimCopy(const std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return std::string{text.substr(first, last - first + 1)};
}

inline std::string toUpperCopy(std::string text)
{
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return text;
}

inline bool parseBool(const std::string_view value, bool& out)
{
    const std::string text = toUpperCopy(trimCopy(value));
    if (text == "ON" || text == "TRUE" || text == "1" || text == "YES") {
        out = true;
        return true;
    }
    if (text == "OFF" || text == "FALSE" || text == "0" || text == "NO") {
        out = false;
        return true;
    }
    return false;
}

inline bool parseInt(const std::string_view value, int& out)
{
    const std::string text = trimCopy(value);
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const int parsed = std::stoi(text, &pos);
        if (pos != text.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

inline bool parseDouble(const std::string_view value, double& out)
{
    const std::string text = trimCopy(value);
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const double parsed = std::stod(text, &pos);
        if (pos != text.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

inline bool parseFloat(const std::string_view value, float& out)
{
    double parsed = 0.0;
    if (!parseDouble(value, parsed)) {
        return false;
    }
    out = static_cast<float>(parsed);
    return true;
}

inline std::string formatToggle(const bool value)
{
    return value ? "ON" : "OFF";
}

inline std::string formatSpeed(double value)
{
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

inline std::string formatFloat(const float value, const char* suffix = "")
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4g%s", value, suffix);
    return buffer;
}

inline std::string formatGravityStrength(const double value)
{
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

inline std::string formatPercent(const double value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0, 1.0) * 100.0);
    return buffer;
}

inline bool isDisabledReasonMessage(const std::string& message)
{
    return message == "ENABLE SHOW MINIMAP FIRST" ||
        message == "ENABLE DRAW PATH FIRST" ||
        message == "ENABLE OBJECT INFO FIRST";
}

inline constexpr int kInterfaceRowUiScale = 0;
inline constexpr int kInterfaceRowMinimapZoom = 5;
inline constexpr int kInterfaceRowDetailLevel = 9;
inline constexpr int kInterfaceRowBackdropPreset = 10;
inline constexpr std::array<const char*, 2> kObjectInfoDetailChoices{{
    "BASIC", "FULL",
}};

inline int objectInfoDetailIndex(const InterfaceSettings& settings)
{
    if (settings.objectInfoMaterial || settings.objectInfoAngularSpeed || settings.objectInfoBodyType) {
        return 1;
    }
    return 0;
}

inline void applyObjectInfoDetailLevel(InterfaceSettings& settings, const int detailIndex)
{
    const int clamped = std::clamp(detailIndex, 0, static_cast<int>(kObjectInfoDetailChoices.size()) - 1);
    settings.objectInfoMaterial = clamped >= 1;
    settings.objectInfoVelocity = true;
    settings.objectInfoMass = true;
    settings.objectInfoRadius = true;
    settings.objectInfoAngularSpeed = clamped >= 1;
    settings.objectInfoBodyType = clamped >= 1;
}

inline void applyInterfacePageSettings(InterfaceSettings& dst, const InterfaceSettings& src)
{
    dst.uiScaleIndex = src.uiScaleIndex;
    dst.minimapZoomIndex = src.minimapZoomIndex;
    dst.showSimulationSpeed = src.showSimulationSpeed;
    dst.showFps = src.showFps;
    dst.showElapsedTime = src.showElapsedTime;
    dst.showMinimap = src.showMinimap;
    dst.showCoordinates = src.showCoordinates;
    dst.showCrosshair = src.showCrosshair;
    dst.backdropPresetIndex = src.backdropPresetIndex;
    dst.objectInfo = src.objectInfo;
    dst.objectInfoMaterial = src.objectInfoMaterial;
    dst.objectInfoVelocity = src.objectInfoVelocity;
    dst.objectInfoMass = src.objectInfoMass;
    dst.objectInfoRadius = src.objectInfoRadius;
    dst.objectInfoAngularSpeed = src.objectInfoAngularSpeed;
    dst.objectInfoBodyType = src.objectInfoBodyType;
}

template <typename T, std::size_t N>
void adjustChoice(const std::array<T, N>& choices, T& value, const int delta)
{
    int idx = closestChoiceIndex(choices, value);
    idx = std::clamp(idx + delta, 0, static_cast<int>(choices.size()) - 1);
    value = choices[static_cast<std::size_t>(idx)];
}

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

inline constexpr std::array<RowDescriptor<DisplaySettings>, 2> kDisplayRows{{
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

inline constexpr std::array<RowDescriptor<SimulationSettings>, 8> kSimulationRows{{
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

inline constexpr std::array<RowDescriptor<CameraSettings>, 4> kCameraRows{{
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

inline constexpr std::array<RowDescriptor<InterfaceSettings>, 11> kInterfaceRows{{
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
    {
        "BACKDROP",
        RowKind::Choice,
        [](const InterfaceSettings& settings) {
            const int index = std::clamp(
                settings.backdropPresetIndex,
                0,
                static_cast<int>(kBackdropPresetChoices.size()) - 1);
            return std::string{kBackdropPresetChoices[static_cast<std::size_t>(index)]};
        },
        nullptr,
        [](InterfaceSettings& settings, int delta) {
            settings.backdropPresetIndex = std::clamp(
                settings.backdropPresetIndex + delta,
                0,
                static_cast<int>(kBackdropPresetChoices.size()) - 1);
        },
    },
}};

inline bool actionAlwaysVisible(SettingsPage, const input::ControlBindings&)
{
    return true;
}

inline bool actionResetWorldVisible(const SettingsPage page, const input::ControlBindings&)
{
    return page == SettingsPage::Simulation;
}

inline bool actionResetControlsVisible(const SettingsPage page, const input::ControlBindings& controls)
{
    return page == SettingsPage::Controls && controls != input::ControlBindings{};
}

inline constexpr std::array<ActionDefinition, 5> kActionDefinitions{{
    {ActionKind::ResetSettings, "", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::Close, "X", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::Exit, "EXIT TO HOME", ActionSection::Top, actionAlwaysVisible},
    {ActionKind::ResetWorld, "RESET WORLD", ActionSection::Bottom, actionResetWorldVisible},
    {ActionKind::ResetControls, "RESET CONTROLS", ActionSection::Bottom, actionResetControlsVisible},
}};

inline constexpr std::array<PopupDefinition, 4> kPopupDefinitions{{
    {PauseMenu::PopupKind::ResetSettings, "ARE YOU SURE?", "RESET ALL SETTINGS", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ResetWorld, "ARE YOU SURE?", "RESET WORLD", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ResetControls, "ARE YOU SURE?", "RESET CONTROLS TO DEFAULTS", "RESET", "CANCEL"},
    {PauseMenu::PopupKind::ExitToHome, "ARE YOU SURE?", "EXIT TO HOME", "EXIT", "CANCEL"},
}};

inline std::vector<const ActionDefinition*> visibleActionsForSection(
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

inline const PopupDefinition* popupDefinitionFor(const PauseMenu::PopupKind kind)
{
    for (const auto& definition : kPopupDefinitions) {
        if (definition.kind == kind) {
            return &definition;
        }
    }
    return nullptr;
}

inline ActionKind actionKindFromId(const int id)
{
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

inline SettingsPage pageFromNumberKey(const int key)
{
    switch (key) {
        case GLFW_KEY_1: return SettingsPage::Display;
        case GLFW_KEY_2: return SettingsPage::Simulation;
        case GLFW_KEY_3: return SettingsPage::Camera;
        case GLFW_KEY_4: return SettingsPage::Interface;
        case GLFW_KEY_5: return SettingsPage::Controls;
        default: return SettingsPage::Display;
    }
}

} // namespace ui::pause_menu_detail

#endif // PHYSICS3D_UI_PAUSEMENUINTERNAL_H

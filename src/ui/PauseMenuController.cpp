#include "ui/PauseMenuController.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ui {
    namespace {
        int wrapIndex(const int idx, const int count) {
            if (count <= 0) {
                return 0;
            }
            const int mod = idx % count;
            return mod < 0 ? (mod + count) : mod;
        }

        using SettingField = PauseMenuController::SettingField;
        using ControlType = OverlayRenderer::PauseMenuControlType;
        using PauseAction = OverlayRenderer::PauseMenuAction;

        constexpr int kFirstSelectableLineIndex = 1;
        constexpr int kToggleOptionCount = 2;

        constexpr std::size_t actionIndex(const PauseAction action) {
            return static_cast<std::size_t>(action);
        }

        template <typename T, std::size_t N>
        constexpr int choiceCount(const std::array<T, N>&) {
            return static_cast<int>(N);
        }

        template <typename T, std::size_t N>
        void stepChoice(const std::array<T, N>& choices, T& value, int delta);

        enum class FieldDependency {
            None = 0,
            ShowHud,
            ObjectInfo,
        };

        enum class AdjustResult {
            Updated = 0,
            OpenEnableDrawPathPopup,
        };

        using SettingValueTextFn = std::string (*)(const PauseMenuSettingsBundle&);
        using SettingOptionIndexFn = int (*)(const PauseMenuSettingsBundle&);
        using SettingAdjustFn = AdjustResult (*)(PauseMenuSettingsBundle&, int);
        using SettingParseConfigFn = bool (*)(PauseMenuSettingsBundle&, std::string_view);
        using SettingSerializeConfigFn = std::string (*)(const PauseMenuSettingsBundle&);

        struct SettingFieldInfo {
            const char* label = "";
            const char* hint = "";
            ControlType controlType = ControlType::None;
            int optionCount = 0;
            FieldDependency dependency = FieldDependency::None;
            SettingValueTextFn valueText = nullptr;
            SettingOptionIndexFn optionIndex = nullptr;
            SettingAdjustFn adjust = nullptr;
            std::string_view configKey{};
            std::string_view legacyConfigKey{};
            SettingParseConfigFn parseConfig = nullptr;
            SettingSerializeConfigFn serializeConfig = nullptr;
        };

        constexpr std::array<SettingField, 2> kDisplayFields{
            SettingField::WindowMode,
            SettingField::Vsync,
        };

        constexpr std::array<SettingField, 8> kSimulationFields{
            SettingField::MinSpeed,
            SettingField::MaxSpeed,
            SettingField::Gravity,
            SettingField::GravityStrength,
            SettingField::Collisions,
            SettingField::VelocityIterations,
            SettingField::PositionIterations,
            SettingField::GlobalBounce,
        };

        constexpr std::array<SettingField, 4> kCameraFields{
            SettingField::LookSensitivity,
            SettingField::BaseMoveSpeed,
            SettingField::InvertY,
            SettingField::Fov,
        };

        constexpr std::array<SettingField, 12> kInterfaceFields{
            SettingField::UiScale,
            SettingField::ShowHud,
            SettingField::Crosshair,
            SettingField::DebugStats,
            SettingField::DrawPath,
            SettingField::ObjectInfo,
            SettingField::ObjectInfoMaterial,
            SettingField::ObjectInfoVelocity,
            SettingField::ObjectInfoMass,
            SettingField::ObjectInfoRadius,
            SettingField::ObjectInfoAngularSpeed,
            SettingField::ObjectInfoBodyType,
        };

        constexpr std::array<std::span<const SettingField>, 5> kFieldsByPage{
            std::span<const SettingField>{kDisplayFields},
            std::span<const SettingField>{kSimulationFields},
            std::span<const SettingField>{kCameraFields},
            std::span<const SettingField>{kInterfaceFields},
            std::span<const SettingField>{},
        };

        constexpr std::array<const char*, 5> kPageHeaders{
            "DISPLAY SETTINGS",
            "SIMULATION SETTINGS",
            "CAMERA SETTINGS",
            "INTERFACE",
            "CONTROLS",
        };

        constexpr std::array<const char*, 2> kWindowModeLabels{
            "BORDERLESS",
            "WINDOWED",
        };

        constexpr bool usesRowDraft(const SettingsPage page) {
            return page == SettingsPage::Simulation ||
                   page == SettingsPage::Camera ||
                   page == SettingsPage::Interface;
        }

        std::string trimCopy(const std::string_view text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos) {
                return "";
            }
            const auto last = text.find_last_not_of(" \t\r\n");
            return std::string{text.substr(first, last - first + 1)};
        }

        std::string toUpperCopy(std::string text) {
            for (char& c : text) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return text;
        }

        bool tryParseBool(const std::string_view value, bool& outValue) {
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

        bool tryParseInt(const std::string_view value, int& outValue) {
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

        bool tryParseDouble(const std::string_view value, double& outValue) {
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

        bool tryParseFloat(const std::string_view value, float& outValue) {
            double parsed = 0.0;
            if (!tryParseDouble(value, parsed)) {
                return false;
            }
            outValue = static_cast<float>(parsed);
            return true;
        }

        bool tryParseWindowMode(const std::string_view value, WindowMode& outMode) {
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

        template <typename Value>
        void writeSettingValue(std::ostream& out, const Value& value) {
            out << value;
        }

        void writeSettingValue(std::ostream& out, const bool value) {
            out << (value ? "ON" : "OFF");
        }

        template <typename Value>
        void writeSetting(std::ostream& out, const std::string_view key, const Value& value) {
            out << key << '=';
            writeSettingValue(out, value);
            out << '\n';
        }

        std::string toggleValueText(const bool enabled) {
            return enabled ? "ON" : "OFF";
        }

        std::string formatWindowModeText(const WindowMode mode) {
            const auto index = static_cast<std::size_t>(mode);
            if (index >= kWindowModeLabels.size()) {
                return "UNKNOWN";
            }
            return kWindowModeLabels[index];
        }

        std::string formatIterationText(const int value) {
            return std::to_string(value);
        }

        std::string formatLookSensitivityText(const float value) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.4f", value);
            return buffer;
        }

        std::string formatBaseMoveSpeedText(const float value) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.0f", value);
            return buffer;
        }

        std::string formatFovText(const float value) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.0f DEG", value);
            return buffer;
        }

        std::string formatUiScaleText(const int value) {
            char buffer[32];
            const int clampedValue = std::clamp(value, 0, static_cast<int>(pause_menu_shared::kUiScaleChoices.size()) - 1);
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%.2fX",
                pause_menu_shared::kUiScaleChoices[static_cast<std::size_t>(clampedValue)]);
            return buffer;
        }

        template <typename T>
        std::string configValueString(const T& value) {
            std::ostringstream out;
            writeSettingValue(out, value);
            return out.str();
        }

        std::string configValueString(const WindowMode value) {
            return formatWindowModeText(value);
        }

        bool parseConfigValue(const std::string_view value, bool& outValue) {
            return tryParseBool(value, outValue);
        }

        bool parseConfigValue(const std::string_view value, int& outValue) {
            return tryParseInt(value, outValue);
        }

        bool parseConfigValue(const std::string_view value, double& outValue) {
            return tryParseDouble(value, outValue);
        }

        bool parseConfigValue(const std::string_view value, float& outValue) {
            return tryParseFloat(value, outValue);
        }

        bool parseConfigValue(const std::string_view value, WindowMode& outValue) {
            return tryParseWindowMode(value, outValue);
        }

        template <typename Settings, auto Member>
        using MemberValueType = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Settings>().*Member)>>;

        template <typename Settings>
        Settings& settingsRef(PauseMenuSettingsBundle& settings);

        template <typename Settings>
        const Settings& settingsRef(const PauseMenuSettingsBundle& settings);

        template <>
        DisplaySettings& settingsRef<DisplaySettings>(PauseMenuSettingsBundle& settings)
        {
            return settings.display;
        }

        template <>
        SimulationSettings& settingsRef<SimulationSettings>(PauseMenuSettingsBundle& settings)
        {
            return settings.simulation;
        }

        template <>
        CameraSettings& settingsRef<CameraSettings>(PauseMenuSettingsBundle& settings)
        {
            return settings.camera;
        }

        template <>
        InterfaceSettings& settingsRef<InterfaceSettings>(PauseMenuSettingsBundle& settings)
        {
            return settings.interface;
        }

        template <>
        const DisplaySettings& settingsRef<DisplaySettings>(const PauseMenuSettingsBundle& settings)
        {
            return settings.display;
        }

        template <>
        const SimulationSettings& settingsRef<SimulationSettings>(const PauseMenuSettingsBundle& settings)
        {
            return settings.simulation;
        }

        template <>
        const CameraSettings& settingsRef<CameraSettings>(const PauseMenuSettingsBundle& settings)
        {
            return settings.camera;
        }

        template <>
        const InterfaceSettings& settingsRef<InterfaceSettings>(const PauseMenuSettingsBundle& settings)
        {
            return settings.interface;
        }

        template <typename Settings, auto Member>
        bool parseFieldConfig(PauseMenuSettingsBundle& settings, const std::string_view value)
        {
            MemberValueType<Settings, Member> parsed{};
            if (!parseConfigValue(value, parsed)) {
                return false;
            }
            settingsRef<Settings>(settings).*Member = parsed;
            return true;
        }

        template <typename Settings, auto Member>
        std::string serializeFieldConfig(const PauseMenuSettingsBundle& settings)
        {
            return configValueString(settingsRef<Settings>(settings).*Member);
        }

        template <typename Settings, auto Member>
        constexpr SettingFieldInfo withConfig(
            SettingFieldInfo info,
            const std::string_view configKey,
            const std::string_view legacyConfigKey = {})
        {
            info.configKey = configKey;
            info.legacyConfigKey = legacyConfigKey;
            info.parseConfig = &parseFieldConfig<Settings, Member>;
            info.serializeConfig = &serializeFieldConfig<Settings, Member>;
            return info;
        }

        AdjustResult adjustMinSpeed(SimulationSettings& simulation, const int delta) {
            stepChoice(pause_menu_shared::kMinSpeedChoices, simulation.minSimSpeed, delta);
            if (simulation.maxSimSpeed < simulation.minSimSpeed) {
                simulation.maxSimSpeed = simulation.minSimSpeed;
            }
            return AdjustResult::Updated;
        }

        AdjustResult adjustMaxSpeed(SimulationSettings& simulation, const int delta) {
            stepChoice(pause_menu_shared::kMaxSpeedChoices, simulation.maxSimSpeed, delta);
            if (simulation.minSimSpeed > simulation.maxSimSpeed) {
                simulation.minSimSpeed = simulation.maxSimSpeed;
            }
            return AdjustResult::Updated;
        }

        AdjustResult adjustDrawPath(InterfaceSettings& interfaceSettings) {
            if (!interfaceSettings.drawPath) {
                return AdjustResult::OpenEnableDrawPathPopup;
            }
            interfaceSettings.drawPath = false;
            return AdjustResult::Updated;
        }

        template <typename Settings, auto Member, auto Adjuster = nullptr>
        constexpr SettingFieldInfo makeToggleField(
            const char* label,
            const char* hint,
            const FieldDependency dependency = FieldDependency::None)
        {
            return {
                label,
                hint,
                ControlType::Toggle,
                kToggleOptionCount,
                dependency,
                +[](const PauseMenuSettingsBundle& settings) {
                    return toggleValueText(settingsRef<Settings>(settings).*Member);
                },
                +[](const PauseMenuSettingsBundle& settings) {
                    return settingsRef<Settings>(settings).*Member ? 1 : 0;
                },
                +[](PauseMenuSettingsBundle& settings, const int) {
                    auto& typedSettings = settingsRef<Settings>(settings);
                    if constexpr (Adjuster == nullptr) {
                        typedSettings.*Member = !(typedSettings.*Member);
                        return AdjustResult::Updated;
                    } else {
                        return Adjuster(typedSettings);
                    }
                },
            };
        }

        template <typename Settings, auto Member, const auto& Choices, auto Formatter, auto Adjuster = nullptr>
        constexpr SettingFieldInfo makeChoiceField(
            const char* label,
            const char* hint,
            const ControlType controlType = ControlType::Numeric,
            const FieldDependency dependency = FieldDependency::None)
        {
            return {
                label,
                hint,
                controlType,
                choiceCount(Choices),
                dependency,
                +[](const PauseMenuSettingsBundle& settings) {
                    return Formatter(settingsRef<Settings>(settings).*Member);
                },
                +[](const PauseMenuSettingsBundle& settings) {
                    return pause_menu_shared::closestChoiceIndex(Choices, settingsRef<Settings>(settings).*Member);
                },
                +[](PauseMenuSettingsBundle& settings, const int delta) {
                    auto& typedSettings = settingsRef<Settings>(settings);
                    if constexpr (Adjuster == nullptr) {
                        stepChoice(Choices, typedSettings.*Member, delta);
                        return AdjustResult::Updated;
                    } else {
                        return Adjuster(typedSettings, delta);
                    }
                },
            };
        }

        template <typename Settings, auto Member, const auto& Labels, auto Formatter>
        constexpr SettingFieldInfo makeEnumChoiceField(const char* label, const char* hint) {
            using Enum = MemberValueType<Settings, Member>;
            return {
                label,
                hint,
                ControlType::Choice,
                choiceCount(Labels),
                FieldDependency::None,
                +[](const PauseMenuSettingsBundle& settings) {
                    return Formatter(settingsRef<Settings>(settings).*Member);
                },
                +[](const PauseMenuSettingsBundle& settings) {
                    return std::clamp(
                        static_cast<int>(settingsRef<Settings>(settings).*Member),
                        0,
                        static_cast<int>(Labels.size()) - 1);
                },
                +[](PauseMenuSettingsBundle& settings, const int delta) {
                    auto& value = settingsRef<Settings>(settings).*Member;
                    value = static_cast<Enum>(wrapIndex(static_cast<int>(value) + delta, static_cast<int>(Labels.size())));
                    return AdjustResult::Updated;
                },
            };
        }

        template <typename Settings, auto Member, const auto& Choices, auto Formatter>
        constexpr SettingFieldInfo makeIndexedChoiceField(
            const char* label,
            const char* hint,
            const FieldDependency dependency = FieldDependency::None)
        {
            return {
                label,
                hint,
                ControlType::Numeric,
                choiceCount(Choices),
                dependency,
                +[](const PauseMenuSettingsBundle& settings) {
                    return Formatter(settingsRef<Settings>(settings).*Member);
                },
                +[](const PauseMenuSettingsBundle& settings) {
                    return std::clamp(
                        settingsRef<Settings>(settings).*Member,
                        0,
                        static_cast<int>(Choices.size()) - 1);
                },
                +[](PauseMenuSettingsBundle& settings, const int delta) {
                    auto& value = settingsRef<Settings>(settings).*Member;
                    value = std::clamp(value + delta, 0, static_cast<int>(Choices.size()) - 1);
                    return AdjustResult::Updated;
                },
            };
        }

        constexpr auto kFieldInfo = std::to_array<SettingFieldInfo>({
            withConfig<DisplaySettings, &DisplaySettings::windowMode>(
                makeEnumChoiceField<DisplaySettings, &DisplaySettings::windowMode, kWindowModeLabels, formatWindowModeText>(
                    "WINDOW MODE",
                    "LEFT/RIGHT TO CYCLE"),
                "WINDOW_MODE"),
            withConfig<DisplaySettings, &DisplaySettings::vsync>(
                makeToggleField<DisplaySettings, &DisplaySettings::vsync>("VSYNC", "TOGGLE"),
                "VSYNC"),
            withConfig<SimulationSettings, &SimulationSettings::minSimSpeed>(
                makeChoiceField<SimulationSettings, &SimulationSettings::minSimSpeed, pause_menu_shared::kMinSpeedChoices, pause_menu_shared::formatSpeedMultiplier, adjustMinSpeed>(
                    "MIN SPEED",
                    "LEFT/RIGHT OR SLIDER"),
                "MIN_SIM_SPEED"),
            withConfig<SimulationSettings, &SimulationSettings::maxSimSpeed>(
                makeChoiceField<SimulationSettings, &SimulationSettings::maxSimSpeed, pause_menu_shared::kMaxSpeedChoices, pause_menu_shared::formatSpeedMultiplier, adjustMaxSpeed>(
                    "MAX SPEED",
                    "LEFT/RIGHT OR SLIDER"),
                "MAX_SIM_SPEED"),
            withConfig<SimulationSettings, &SimulationSettings::gravityEnabled>(
                makeToggleField<SimulationSettings, &SimulationSettings::gravityEnabled>("GRAVITY", "TOGGLE"),
                "GRAVITY_ENABLED"),
            withConfig<SimulationSettings, &SimulationSettings::gravityStrength>(
                makeChoiceField<SimulationSettings, &SimulationSettings::gravityStrength, pause_menu_shared::kGravityStrengthChoices, pause_menu_shared::formatGravityMultiplier>(
                    "GRAVITY STRENGTH",
                    "LEFT/RIGHT OR SLIDER"),
                "GRAVITY_STRENGTH"),
            withConfig<SimulationSettings, &SimulationSettings::collisionsEnabled>(
                makeToggleField<SimulationSettings, &SimulationSettings::collisionsEnabled>("COLLISIONS", "TOGGLE"),
                "COLLISIONS_ENABLED"),
            withConfig<SimulationSettings, &SimulationSettings::velocityIterations>(
                makeChoiceField<SimulationSettings, &SimulationSettings::velocityIterations, pause_menu_shared::kCollisionIterationChoices, formatIterationText>(
                    "VELOCITY ITERATIONS",
                    "LEFT/RIGHT OR SLIDER"),
                "VELOCITY_ITERATIONS",
                "COLLISION_ITERATIONS"),
            withConfig<SimulationSettings, &SimulationSettings::positionIterations>(
                makeChoiceField<SimulationSettings, &SimulationSettings::positionIterations, pause_menu_shared::kCollisionIterationChoices, formatIterationText>(
                    "POSITION ITERATIONS",
                    "LEFT/RIGHT OR SLIDER"),
                "POSITION_ITERATIONS"),
            withConfig<SimulationSettings, &SimulationSettings::globalRestitution>(
                makeChoiceField<SimulationSettings, &SimulationSettings::globalRestitution, pause_menu_shared::kGlobalRestitutionChoices, pause_menu_shared::formatRestitutionPercent>(
                    "GLOBAL BOUNCE",
                    "LEFT/RIGHT OR SLIDER"),
                "GLOBAL_RESTITUTION"),
            withConfig<CameraSettings, &CameraSettings::lookSensitivity>(
                makeChoiceField<CameraSettings, &CameraSettings::lookSensitivity, pause_menu_shared::kLookSensitivityChoices, formatLookSensitivityText>(
                    "LOOK SENSITIVITY",
                    "LEFT/RIGHT OR SLIDER"),
                "LOOK_SENSITIVITY"),
            withConfig<CameraSettings, &CameraSettings::baseMoveSpeed>(
                makeChoiceField<CameraSettings, &CameraSettings::baseMoveSpeed, pause_menu_shared::kBaseMoveSpeedChoices, formatBaseMoveSpeedText>(
                    "BASE MOVE SPEED",
                    "LEFT/RIGHT OR SLIDER"),
                "BASE_MOVE_SPEED"),
            withConfig<CameraSettings, &CameraSettings::invertY>(
                makeToggleField<CameraSettings, &CameraSettings::invertY>("INVERT Y", "TOGGLE"),
                "INVERT_Y"),
            withConfig<CameraSettings, &CameraSettings::fovDegrees>(
                makeChoiceField<CameraSettings, &CameraSettings::fovDegrees, pause_menu_shared::kFovChoices, formatFovText>(
                    "FOV",
                    "LEFT/RIGHT OR SLIDER"),
                "FOV_DEGREES"),
            withConfig<InterfaceSettings, &InterfaceSettings::uiScaleIndex>(
                makeIndexedChoiceField<InterfaceSettings, &InterfaceSettings::uiScaleIndex, pause_menu_shared::kUiScaleChoices, formatUiScaleText>(
                    "UI SCALE",
                    "LEFT/RIGHT OR SLIDER"),
                "UI_SCALE_INDEX"),
            withConfig<InterfaceSettings, &InterfaceSettings::showHud>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::showHud>("SHOW HUD", "TOGGLE"),
                "SHOW_HUD"),
            withConfig<InterfaceSettings, &InterfaceSettings::showCrosshair>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::showCrosshair>("CROSSHAIR", "TOGGLE"),
                "SHOW_CROSSHAIR"),
            withConfig<InterfaceSettings, &InterfaceSettings::showPhysicsStats>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::showPhysicsStats>("DEBUG STATS", "TOGGLE", FieldDependency::ShowHud),
                "SHOW_DEBUG_STATS",
                "SHOW_PHYSICS_STATS"),
            withConfig<InterfaceSettings, &InterfaceSettings::drawPath>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::drawPath, adjustDrawPath>(
                    "DRAW PATH",
                    "TOGGLE - MAY IMPACT PERFORMANCE"),
                "DRAW_PATH"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfo>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfo>("OBJECT INFO", "TOGGLE"),
                "OBJECT_INFO"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoMaterial>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoMaterial>("OBJECT INFO MATERIAL", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_MATERIAL"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoVelocity>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoVelocity>("OBJECT INFO VELOCITY", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_VELOCITY"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoMass>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoMass>("OBJECT INFO MASS", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_MASS"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoRadius>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoRadius>("OBJECT INFO RADIUS", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_RADIUS"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoAngularSpeed>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoAngularSpeed>("OBJECT INFO ANGULAR SPEED", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_ANGULAR_SPEED"),
            withConfig<InterfaceSettings, &InterfaceSettings::objectInfoBodyType>(
                makeToggleField<InterfaceSettings, &InterfaceSettings::objectInfoBodyType>("OBJECT INFO BODY TYPE", "TOGGLE", FieldDependency::ObjectInfo),
                "OBJECT_INFO_BODY_TYPE"),
        });

        const SettingFieldInfo& fieldInfo(const SettingField field) {
            return kFieldInfo[static_cast<std::size_t>(field)];
        }

        const char* dependencyReason(const FieldDependency dependency) {
            switch (dependency) {
                case FieldDependency::None:
                    return "SETTING LOCKED";
                case FieldDependency::ShowHud:
                    return "ENABLE SHOW HUD FIRST";
                case FieldDependency::ObjectInfo:
                    return "ENABLE OBJECT INFO FIRST";
            }
            return "SETTING LOCKED";
        }

        bool dependencySatisfied(const FieldDependency dependency, const InterfaceSettings& settings) {
            switch (dependency) {
                case FieldDependency::None:
                    return true;
                case FieldDependency::ShowHud:
                    return settings.showHud;
                case FieldDependency::ObjectInfo:
                    return settings.objectInfo;
            }
            return true;
        }

        template <typename T, std::size_t N>
        void stepChoice(const std::array<T, N>& choices, T& value, const int delta) {
            const int idx = std::clamp(pause_menu_shared::closestChoiceIndex(choices, value) + delta, 0, static_cast<int>(N) - 1);
            value = choices[static_cast<std::size_t>(idx)];
        }

        float normalizedOption(const int idx, const int count) {
            if (count <= 1) {
                return 0.0f;
            }
            return std::clamp(static_cast<float>(idx) / static_cast<float>(count - 1), 0.0f, 1.0f);
        }
    } // namespace

    using namespace pause_menu_shared;

    float PauseMenuController::uiScale() const {
        return uiScaleValue(appliedSettings_.interface.uiScaleIndex);
    }

    std::span<const PauseMenuController::SettingField> PauseMenuController::settingFieldsForPage(const SettingsPage page) {
        const auto index = static_cast<std::size_t>(page);
        if (index >= kFieldsByPage.size()) {
            return {};
        }
        return kFieldsByPage[index];
    }

    PauseMenuController::SettingField PauseMenuController::settingFieldForRow(const int row) const {
        const auto fields = settingFieldsForPage(settingsPage_);
        if (row < 0 || static_cast<std::size_t>(row) >= fields.size()) {
            return SettingField::WindowMode;
        }
        return fields[static_cast<std::size_t>(row)];
    }

    int PauseMenuController::settingCount() const {
        return isControlPage() ? controlCount() : static_cast<int>(settingFieldsForPage(settingsPage_).size());
    }

    int PauseMenuController::controlCount() {
        return static_cast<int>(input::rebindActions().size());
    }

    float PauseMenuController::uiScaleValue(int idx) {
        idx = std::clamp(idx, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
        return kUiScaleChoices[static_cast<std::size_t>(idx)];
    }

    bool PauseMenuController::isControlPage() const {
        return settingsPage_ == SettingsPage::Controls;
    }

    OverlayRenderer::PauseMenuHud PauseMenuController::buildHud(
        const input::ControlBindings& controls,
        const bool advanceApplyIndicator) const
    {
        OverlayRenderer::PauseMenuHud hud{};
        hud.visible = open_;
        if (!open_) {
            return hud;
        }

        hud.awaitingBind = awaitingRebind_;
        hud.selectedRowIsControl = isControlPage();
        hud.firstVisibleLineIndex = std::max(0, scrollLineOffset_);
        const bool controlsDirty = controls != input::ControlBindings{};
        const bool pendingSettings = hasPendingSettingsChanges();

        const FocusTarget anchoredFocusTarget = hasOpenPopup() ? popupReturnTarget_ : focusTarget_;
        constexpr std::array<PauseAction, 6> kHudActions{
            PauseAction::Apply,
            PauseAction::ResetWorld,
            PauseAction::ResetControls,
            PauseAction::ResetIcon,
            PauseAction::Close,
            PauseAction::Exit,
        };
        const auto actionVisible = [&](const PauseAction action) {
            switch (action) {
                case PauseAction::Apply:
                    return pendingSettings;
                case PauseAction::ResetWorld:
                    return settingsPage_ == SettingsPage::Simulation;
                case PauseAction::ResetControls:
                    return settingsPage_ == SettingsPage::Controls && controlsDirty;
                case PauseAction::ResetIcon:
                case PauseAction::Close:
                case PauseAction::Exit:
                    return true;
                case PauseAction::Count:
                    return false;
            }
            return false;
        };
        const auto actionFocusTarget = [](const PauseAction action) {
            switch (action) {
                case PauseAction::Apply:
                    return FocusTarget::ApplyAction;
                case PauseAction::ResetWorld:
                    return FocusTarget::ResetWorldAction;
                case PauseAction::ResetControls:
                    return FocusTarget::ResetControlsAction;
                case PauseAction::ResetIcon:
                    return FocusTarget::ResetIcon;
                case PauseAction::Close:
                    return FocusTarget::CloseButton;
                case PauseAction::Exit:
                    return FocusTarget::ExitButton;
                case PauseAction::Count:
                    return FocusTarget::SettingsRow;
            }
            return FocusTarget::SettingsRow;
        };
        for (const auto action : kHudActions) {
            const auto idx = actionIndex(action);
            hud.actions[idx] = {
                .visible = actionVisible(action),
                .hovered = hoveredActions_[idx],
                .selected = anchoredFocusTarget == actionFocusTarget(action),
            };
        }
        hud.hoveredPageTabIndex = hoveredPageTabIndex_;
        hud.showResetConfirm = hasOpenPopup();
        hud.hoverResetConfirmYes = hoveredResetConfirmYes_;
        hud.hoverResetConfirmNo = hoveredResetConfirmNo_;
        hud.selectedResetConfirmYes = focusTarget_ == FocusTarget::PopupYes;
        hud.selectedResetConfirmNo = focusTarget_ == FocusTarget::PopupNo;
        if (const PopupModel popup = popupModel(); hud.showResetConfirm) {
            hud.resetConfirmSingleAcknowledge = popup.singleAcknowledge;
            hud.resetConfirmTitleText = popup.titleText;
            hud.resetConfirmBodyText = popup.bodyText;
            hud.resetConfirmYesLabel = popup.yesLabel;
            hud.resetConfirmNoLabel = popup.noLabel;
        }

        hud.footerHint =
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE VALUE   ENTER: APPLY OR REBIND   TAB: CHANGE PAGE   ESC: CLOSE";

        if (awaitingRebind_ && awaitingRebindAction_ >= 0 && awaitingRebindAction_ < controlCount()) {
            hud.pendingAction = input::rebindActions()[awaitingRebindAction_].label;
        }

        hud.statusLine = statusMessage_;
        hud.activePageIndex = static_cast<int>(settingsPage_);
        const auto& settings = activeSettings();
        const auto fieldValueText = [&](const SettingField field) {
            const auto& info = fieldInfo(field);
            return info.valueText ? info.valueText(settings) : std::string{};
        };
        const auto fieldOptionIndex = [&](const SettingField field) {
            const auto& info = fieldInfo(field);
            return info.optionIndex ? info.optionIndex(settings) : 0;
        };
        if (hud.statusLine.empty()) {
            const int rowForHint = hoveredSettingRow_ >= 0 ? hoveredSettingRow_ : selectedSettingRow_;
            if (!isControlPage() && rowForHint >= 0 && isSettingRowDisabled(rowForHint)) {
                hud.statusLine = disabledReasonForRow(rowForHint);
            }
        }

        OverlayRenderer::PauseMenuHudLine header{};
        header.text = kPageHeaders[static_cast<std::size_t>(settingsPage_)];
        header.header = true;
        hud.lines.push_back(header);

        const int rowCount = settingCount();
        hud.lines.reserve(static_cast<std::size_t>(rowCount + 1));
        if (isControlPage()) {
            for (int row = 0; row < rowCount; ++row) {
                const auto& action = input::rebindActions()[row];
                OverlayRenderer::PauseMenuHudLine line{};
                line.text = action.label;
                line.valueText = input::keyNameForCode(controls.*(action.member));
                line.hintText = "ENTER/SPACE OR CLICK TO REBIND";
                line.controlType = OverlayRenderer::PauseMenuControlType::Rebind;
                hud.lines.push_back(line);
            }
        } else {
            for (int row = 0; row < rowCount; ++row) {
                const SettingField field = settingFieldForRow(row);
                const auto& info = fieldInfo(field);
                const int optionIndex = fieldOptionIndex(field);
                OverlayRenderer::PauseMenuHudLine line{};
                line.text = info.label;
                line.valueText = fieldValueText(field);
                line.disabled = isSettingRowDisabled(row);
                line.controlType = info.controlType;
                line.boolValue = info.controlType == ControlType::Toggle && optionIndex > 0;
                line.showArrows = line.controlType == ControlType::Choice || line.controlType == ControlType::Numeric;
                line.showSlider = line.controlType == ControlType::Numeric;
                line.sliderT = normalizedOption(optionIndex, info.optionCount);
                line.hintText = line.disabled ? disabledReasonForRow(row) : info.hint;
                hud.lines.push_back(line);
            }
        }

        const bool showSelectedLine =
            (focusTarget_ == FocusTarget::SettingsRow ||
             (hasOpenPopup() && popupModel().keepSelectedLine)) &&
            selectedSettingRow_ >= 0 &&
            rowCount > 0;
        if (showSelectedLine) {
            hud.selectedSettingLineIndex =
                kFirstSelectableLineIndex + std::clamp(selectedSettingRow_, 0, rowCount - 1);
        }
        if (hoveredSettingRow_ >= 0 && rowCount > 0) {
            hud.hoveredSettingLineIndex =
                kFirstSelectableLineIndex + std::clamp(hoveredSettingRow_, 0, rowCount - 1);
        }
        if (applyIndicatorFrames_ > 0 && lastAppliedPage_ == settingsPage_ && rowCount > 0 && lastAppliedRow_ >= 0) {
            hud.appliedSettingLineIndex =
                kFirstSelectableLineIndex + std::clamp(lastAppliedRow_, 0, rowCount - 1);
        }

        if (advanceApplyIndicator && applyIndicatorFrames_ > 0) {
            --applyIndicatorFrames_;
            if (applyIndicatorFrames_ == 0) {
                lastAppliedRow_ = -1;
            }
        }

        return hud;
    }

    void PauseMenuController::clearHoverState() {
        hoveredSettingRow_ = -1;
        hoveredActions_.fill(false);
        hoveredPageTabIndex_ = -1;
        hoveredResetConfirmYes_ = false;
        hoveredResetConfirmNo_ = false;
    }

    const PauseMenuSettingsBundle& PauseMenuController::activeSettings() const {
        if (settingsPage_ == SettingsPage::Display) {
            return hasPendingPageChanges(SettingsPage::Display) ? draftSettings_ : appliedSettings_;
        }
        return (pendingSelectionEdit_ && draftSettingsPage_ == settingsPage_) ? draftSettings_ : appliedSettings_;
    }

    void PauseMenuController::setSelectedSettingRow(const int row) {
        selectedSettingRow_ = row;
        if (row >= 0) {
            focusTarget_ = FocusTarget::SettingsRow;
        }
    }

    void PauseMenuController::restoreDraftFromApplied() {
        draftSettings_ = appliedSettings_;
    }

    bool PauseMenuController::hasOpenPopup() const {
        return popupType_ != PopupType::None;
    }

    PauseMenuController::PopupModel PauseMenuController::popupModel() const {
        switch (popupType_) {
            case PopupType::ResetSettings:
                return {
                    .titleText = "ARE YOU SURE?",
                    .bodyText = "RESET ALL SETTINGS",
                    .yesLabel = "RESET",
                    .noLabel = "CANCEL",
                    .singleAcknowledge = false,
                    .keepSelectedLine = false,
                    .restoreFocusOnConfirm = false,
                    .confirmAction = PopupConfirmAction::ResetSettings,
                };
            case PopupType::ResetWorld:
                return {
                    .titleText = "ARE YOU SURE?",
                    .bodyText = "RESET WORLD",
                    .yesLabel = "RESET",
                    .noLabel = "CANCEL",
                    .singleAcknowledge = false,
                    .keepSelectedLine = true,
                    .restoreFocusOnConfirm = false,
                    .confirmAction = PopupConfirmAction::ResetWorld,
                };
            case PopupType::ExitToHome:
                return {
                    .titleText = "ARE YOU SURE?",
                    .bodyText = "EXIT TO HOME",
                    .yesLabel = "EXIT",
                    .noLabel = "CANCEL",
                    .singleAcknowledge = false,
                    .keepSelectedLine = false,
                    .restoreFocusOnConfirm = false,
                    .confirmAction = PopupConfirmAction::ExitToHome,
                };
            case PopupType::EnableDrawPath:
                return {
                    .titleText = "WARNING",
                    .bodyText = "MAY IMPACT PERFORMANCE",
                    .yesLabel = "I UNDERSTAND",
                    .noLabel = "CANCEL",
                    .singleAcknowledge = true,
                    .keepSelectedLine = true,
                    .restoreFocusOnConfirm = true,
                    .confirmAction = PopupConfirmAction::EnableDrawPath,
                };
            case PopupType::None:
                return {};
        }
        return {};
    }

    void PauseMenuController::openPopup(
        const PopupType type,
        const FocusTarget returnTarget,
        const bool selectDefaultAction)
    {
        popupType_ = type;
        popupReturnTarget_ = returnTarget;
        clearHoverState();
        focusTarget_ = selectDefaultAction ? FocusTarget::PopupYes : returnTarget;
    }

    void PauseMenuController::closePopup(const bool restoreFocus) {
        popupType_ = PopupType::None;
        clearHoverState();
        focusTarget_ = restoreFocus ? popupReturnTarget_ : FocusTarget::SettingsRow;
    }

    void PauseMenuController::confirmPopup(GLFWwindow* window) {
        const PopupModel popup = popupModel();
        switch (popup.confirmAction) {
            case PopupConfirmAction::EnableDrawPath:
                ensureDraftForSelectedRow();
                draftSettings_.interface.drawPath = true;
                pendingSelectionEdit_ = hasPendingSelectionChanges();
                if (!pendingSelectionEdit_) {
                    draftSelectionRow_ = -1;
                    draftSettingsPage_ = settingsPage_;
                }
                statusMessage_.clear();
                closePopup(popup.restoreFocusOnConfirm);
                return;
            case PopupConfirmAction::ResetWorld:
                resetWorldRequested_ = true;
                statusMessage_ = "WORLD RESET";
                closePopup(popup.restoreFocusOnConfirm);
                return;
            case PopupConfirmAction::ExitToHome:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                closePopup(popup.restoreFocusOnConfirm);
                return;
            case PopupConfirmAction::ResetSettings:
                resetAllSettings(window);
                closePopup(popup.restoreFocusOnConfirm);
                return;
            case PopupConfirmAction::None:
                return;
        }
    }

    void PauseMenuController::discardPendingEdit() {
        restoreDraftFromApplied();
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
            restoreDraftFromApplied();
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
        clearHoverState();
        popupType_ = PopupType::None;
        popupReturnTarget_ = FocusTarget::SettingsRow;

        setSelectedSettingRow(-1);
        scrollLineOffset_ = 0;
        statusMessage_.clear();
    }

    void PauseMenuController::commitSelectedSetting(GLFWwindow* window) {
        if (isControlPage()) {
            return;
        }

        const bool rowCommitReady =
            pendingSelectionEdit_ &&
            draftSettingsPage_ == settingsPage_ &&
            draftSelectionRow_ == selectedSettingRow_;
        switch (settingsPage_) {
            case SettingsPage::Display:
                if (!hasPendingPageChanges(SettingsPage::Display)) {
                    statusMessage_.clear();
                    return;
                }
                appliedSettings_.display = draftSettings_.display;
                applyDisplaySettings(window, appliedSettings_.display);
                break;
            case SettingsPage::Simulation:
                if (!rowCommitReady) {
                    statusMessage_.clear();
                    return;
                }
                appliedSettings_.simulation = draftSettings_.simulation;
                break;
            case SettingsPage::Camera:
                if (!rowCommitReady) {
                    statusMessage_.clear();
                    return;
                }
                appliedSettings_.camera = draftSettings_.camera;
                break;
            case SettingsPage::Interface:
                if (!rowCommitReady) {
                    statusMessage_.clear();
                    return;
                }
                appliedSettings_.interface = draftSettings_.interface;
                break;
            case SettingsPage::Controls:
                return;
        }
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();

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

    void PauseMenuController::moveVerticalFocus(const int delta, const bool controlsDirty) {
        if (hasOpenPopup()) {
            if (focusTarget_ != FocusTarget::PopupYes && focusTarget_ != FocusTarget::PopupNo) {
                focusTarget_ = FocusTarget::PopupYes;
            }
            return;
        }

        const int rowCount = settingCount();
        if (rowCount <= 0 && focusTarget_ == FocusTarget::SettingsRow) {
            return;
        }

        constexpr std::array<FocusTarget, 3> kTopOrder{
            FocusTarget::ResetIcon,
            FocusTarget::CloseButton,
            FocusTarget::ExitButton,
        };

        std::array<FocusTarget, 2> bottomOrder{};
        int bottomCount = 0;
        if (settingsPage_ == SettingsPage::Simulation) {
            bottomOrder[bottomCount++] = FocusTarget::ResetWorldAction;
        } else if (settingsPage_ == SettingsPage::Controls) {
            if (controlsDirty) {
                bottomOrder[bottomCount++] = FocusTarget::ResetControlsAction;
            }
        }
        if (settingsPage_ != SettingsPage::Controls && hasPendingSettingsChanges()) {
            bottomOrder[bottomCount++] = FocusTarget::ApplyAction;
        }

        const auto findFocus = [](const auto& order, const int count, const FocusTarget target) {
            for (int i = 0; i < count; ++i) {
                if (order[static_cast<std::size_t>(i)] == target) {
                    return i;
                }
            }
            return -1;
        };

        if (focusTarget_ == FocusTarget::SettingsRow) {
            discardPendingEdit();
            if (delta > 0) {
                if (selectedSettingRow_ >= rowCount - 1) {
                    if (bottomCount > 0) {
                        focusTarget_ = bottomOrder[0];
                    }
                } else {
                    setSelectedSettingRow(selectedSettingRow_ + 1);
                }
            } else if (selectedSettingRow_ <= 0) {
                focusTarget_ = FocusTarget::ExitButton;
            } else {
                setSelectedSettingRow(selectedSettingRow_ - 1);
            }
            statusMessage_.clear();
            return;
        }

        if (const int topIndex = findFocus(kTopOrder, static_cast<int>(kTopOrder.size()), focusTarget_); topIndex >= 0) {
            if (delta > 0) {
                if (topIndex + 1 < static_cast<int>(kTopOrder.size())) {
                    focusTarget_ = kTopOrder[static_cast<std::size_t>(topIndex + 1)];
                } else if (rowCount > 0) {
                    setSelectedSettingRow(0);
                }
            } else if (topIndex > 0) {
                focusTarget_ = kTopOrder[static_cast<std::size_t>(topIndex - 1)];
            }
            statusMessage_.clear();
            return;
        }

        if (const int bottomIndex = findFocus(bottomOrder, bottomCount, focusTarget_); bottomIndex >= 0) {
            if (delta > 0) {
                if (bottomIndex + 1 < bottomCount) {
                    focusTarget_ = bottomOrder[static_cast<std::size_t>(bottomIndex + 1)];
                }
            } else if (bottomIndex > 0) {
                focusTarget_ = bottomOrder[static_cast<std::size_t>(bottomIndex - 1)];
            } else if (rowCount > 0) {
                setSelectedSettingRow(rowCount - 1);
            }
            statusMessage_.clear();
        }
    }

    void PauseMenuController::moveHorizontalFocus(const int delta) {
        if (hasOpenPopup()) {
            focusTarget_ = (delta < 0 || popupModel().singleAcknowledge) ? FocusTarget::PopupYes : FocusTarget::PopupNo;
            return;
        }
        if (focusTarget_ == FocusTarget::SettingsRow && selectedSettingRow_ >= 0 && !isControlPage()) {
            adjustSelectedSetting(delta);
        }
    }

    void PauseMenuController::resetControlsToDefaults(
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        controls = input::ControlBindings{};
        input::saveControlBindings(controls, controlsConfigPath);
        markAppliedSelection();
        statusMessage_.clear();
    }

    void PauseMenuController::activateFocusedControl(
        GLFWwindow* window,
        input::ControlBindings& controls,
        const std::string& controlsConfigPath)
    {
        if (hasOpenPopup()) {
            if (focusTarget_ == FocusTarget::PopupYes) {
                confirmPopup(window);
            } else if (focusTarget_ == FocusTarget::PopupNo) {
                closePopup();
            }
            return;
        }

        if (focusTarget_ == FocusTarget::SettingsRow) {
            if (selectedSettingRow_ < 0) {
                return;
            }
            if (isControlPage()) {
                beginRebindForRow(selectedSettingRow_);
            } else {
                commitSelectedSetting(window);
            }
            return;
        }

        switch (focusTarget_) {
            case FocusTarget::ResetIcon:
                openPopup(PopupType::ResetSettings, FocusTarget::ResetIcon, true);
                return;
            case FocusTarget::CloseButton:
                resumeRequested_ = true;
                return;
            case FocusTarget::ExitButton:
                openPopup(PopupType::ExitToHome, FocusTarget::ExitButton, true);
                return;
            case FocusTarget::ApplyAction:
                commitSelectedSetting(window);
                return;
            case FocusTarget::ResetWorldAction:
                openPopup(PopupType::ResetWorld, FocusTarget::ResetWorldAction, true);
                return;
            case FocusTarget::ResetControlsAction:
                resetControlsToDefaults(controls, controlsConfigPath);
                return;
            case FocusTarget::PopupYes:
            case FocusTarget::PopupNo:
            case FocusTarget::SettingsRow:
                return;
        }
    }

    bool PauseMenuController::isSettingRowDisabled(const int row) const {
        if (row < 0) {
            return true;
        }
        const auto dependency = fieldInfo(settingFieldForRow(row)).dependency;
        return dependency != FieldDependency::None && !dependencySatisfied(dependency, activeSettings().interface);
    }

    std::string PauseMenuController::disabledReasonForRow(const int row) const {
        return dependencyReason(fieldInfo(settingFieldForRow(row)).dependency);
    }

    void PauseMenuController::adjustSelectedSetting(const int delta, const bool selectPopupDefaultAction) {
        if (isControlPage()) {
            return;
        }

        if (selectedSettingRow_ < 0) {
            return;
        }
        if (isSettingRowDisabled(selectedSettingRow_)) {
            statusMessage_ = disabledReasonForRow(selectedSettingRow_);
            return;
        }

        const auto& info = fieldInfo(settingFieldForRow(selectedSettingRow_));

        if (settingsPage_ == SettingsPage::Display) {
            const bool hadChanges = hasPendingPageChanges(SettingsPage::Display);
            if (!hadChanges) {
                draftSettings_ = appliedSettings_;
            }
            if (info.adjust) {
                info.adjust(draftSettings_, delta);
            }
            if (!hasPendingPageChanges(SettingsPage::Display)) {
                draftSettings_ = appliedSettings_;
            }
            statusMessage_.clear();
            return;
        }

        ensureDraftForSelectedRow();
        if (info.adjust && info.adjust(draftSettings_, delta) == AdjustResult::OpenEnableDrawPathPopup)
        {
            openPopup(PopupType::EnableDrawPath, FocusTarget::SettingsRow, selectPopupDefaultAction);
            statusMessage_.clear();
            return;
        }

        statusMessage_.clear();
        pendingSelectionEdit_ = hasPendingSelectionChanges();
        if (!pendingSelectionEdit_) {
            draftSelectionRow_ = -1;
            draftSettingsPage_ = settingsPage_;
        }
    }

    void PauseMenuController::resetAllSettings(GLFWwindow* window) {
        appliedSettings_ = PauseMenuSettingsBundle{};
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedSettings_.display);
        discardPendingEdit();
        markAppliedSelection();
        statusMessage_.clear();
        saveSettings();
    }

    bool PauseMenuController::consumeResetWorldRequest() {
        const bool requested = resetWorldRequested_;
        resetWorldRequested_ = false;
        return requested;
    }

    bool PauseMenuController::hasPendingPageChanges(const SettingsPage page) const {
        switch (page) {
            case SettingsPage::Display:
                return draftSettings_.display != appliedSettings_.display;
            case SettingsPage::Simulation:
                return draftSettings_.simulation != appliedSettings_.simulation;
            case SettingsPage::Camera:
                return draftSettings_.camera != appliedSettings_.camera;
            case SettingsPage::Interface:
                return draftSettings_.interface != appliedSettings_.interface;
            case SettingsPage::Controls:
                return false;
        }
        return false;
    }

    bool PauseMenuController::hasPendingSelectionChanges() const {
        return pendingSelectionEdit_ &&
               usesRowDraft(draftSettingsPage_) &&
               hasPendingPageChanges(draftSettingsPage_);
    }

    bool PauseMenuController::hasPendingSettingsChanges() const {
        if (isControlPage()) {
            return false;
        }
        if (settingsPage_ == SettingsPage::Display) {
            return hasPendingPageChanges(SettingsPage::Display);
        }
        return pendingSelectionEdit_ &&
               draftSettingsPage_ == settingsPage_ &&
               draftSelectionRow_ == selectedSettingRow_ &&
               hasPendingSelectionChanges();
    }

    void PauseMenuController::markAppliedSelection() const {
        lastAppliedPage_ = settingsPage_;
        lastAppliedRow_ = selectedSettingRow_;
        applyIndicatorFrames_ = 120;
    }
} // namespace ui

namespace ui {
    namespace {
        using namespace pause_menu_shared;

        constexpr int kMinWindowWidth = 960;
        constexpr int kMinWindowHeight = 540;

        template <typename Settings, typename Value>
        struct SettingBinding {
            std::string_view key;
            Value Settings::*field;
            bool (*parse)(std::string_view, Value&);
            std::string_view legacyKey{};
        };

        template <typename Settings, typename Value, std::size_t N>
        bool tryLoadBindings(
            const std::string_view key,
            const std::string_view value,
            Settings& settings,
            const std::array<SettingBinding<Settings, Value>, N>& bindings)
        {
            for (const auto& binding : bindings) {
                if (key != binding.key && (binding.legacyKey.empty() || key != binding.legacyKey)) {
                    continue;
                }

                Value parsed{};
                if (binding.parse(value, parsed)) {
                    settings.*(binding.field) = parsed;
                }
                return true;
            }
            return false;
        }

        template <typename T, std::size_t N>
        void snapChoice(const std::array<T, N>& choices, T& value) {
            value = choices[static_cast<std::size_t>(closestChoiceIndex(choices, value))];
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

        constexpr std::array<SettingBinding<DisplaySettings, int>, 4> kDisplayIntBindings{{
            {"WINDOWED_X", &DisplaySettings::windowedX, tryParseInt},
            {"WINDOWED_Y", &DisplaySettings::windowedY, tryParseInt},
            {"WINDOWED_WIDTH", &DisplaySettings::windowedWidth, tryParseInt},
            {"WINDOWED_HEIGHT", &DisplaySettings::windowedHeight, tryParseInt},
        }};

        bool tryLoadFieldConfig(const std::string_view key, const std::string_view value, PauseMenuSettingsBundle& settings)
        {
            for (const auto& info : kFieldInfo) {
                if (info.configKey.empty()) {
                    continue;
                }
                if (key != info.configKey && (info.legacyConfigKey.empty() || key != info.legacyConfigKey)) {
                    continue;
                }
                if (info.parseConfig) {
                    info.parseConfig(settings, value);
                }
                return true;
            }
            return false;
        }

        void writeFieldConfig(std::ostream& out, const SettingFieldInfo& info, const PauseMenuSettingsBundle& settings)
        {
            if (info.configKey.empty() || !info.serializeConfig) {
                return;
            }
            out << info.configKey << '=' << info.serializeConfig(settings) << '\n';
        }
    } // namespace

    void PauseMenuController::loadSettings(const std::string& path) {
        settingsConfigPath_ = path;

        std::ifstream in(path);
        if (in) {
            const auto tryLoadSetting = [&](const std::string_view key, const std::string_view value) {
                return tryLoadBindings(key, value, appliedSettings_.display, kDisplayIntBindings) ||
                       tryLoadFieldConfig(key, value, appliedSettings_);
            };

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

                tryLoadSetting(key, value);
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

        writeFieldConfig(
            out,
            fieldInfo(SettingField::WindowMode),
            appliedSettings_);
        writeSetting(out, "WINDOWED_X", appliedSettings_.display.windowedX);
        writeSetting(out, "WINDOWED_Y", appliedSettings_.display.windowedY);
        writeSetting(out, "WINDOWED_WIDTH", appliedSettings_.display.windowedWidth);
        writeSetting(out, "WINDOWED_HEIGHT", appliedSettings_.display.windowedHeight);
        for (std::size_t i = 1; i < kFieldInfo.size(); ++i) {
            writeFieldConfig(out, kFieldInfo[i], appliedSettings_);
        }
    }

    void PauseMenuController::applyCurrentDisplaySettings(GLFWwindow* window) {
        normalizeAppliedSettings();
        applyDisplaySettings(window, appliedSettings_.display);
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

    void PauseMenuController::normalizeAppliedSettings() {
        snapChoice(kMinSpeedChoices, appliedSettings_.simulation.minSimSpeed);
        snapChoice(kMaxSpeedChoices, appliedSettings_.simulation.maxSimSpeed);
        snapChoice(kGravityStrengthChoices, appliedSettings_.simulation.gravityStrength);
        snapChoice(kCollisionIterationChoices, appliedSettings_.simulation.velocityIterations);
        snapChoice(kCollisionIterationChoices, appliedSettings_.simulation.positionIterations);
        snapChoice(kGlobalRestitutionChoices, appliedSettings_.simulation.globalRestitution);
        if (appliedSettings_.simulation.maxSimSpeed < appliedSettings_.simulation.minSimSpeed) {
            appliedSettings_.simulation.maxSimSpeed = appliedSettings_.simulation.minSimSpeed;
        }

        snapChoice(kLookSensitivityChoices, appliedSettings_.camera.lookSensitivity);
        snapChoice(kBaseMoveSpeedChoices, appliedSettings_.camera.baseMoveSpeed);
        snapChoice(kFovChoices, appliedSettings_.camera.fovDegrees);

        appliedSettings_.interface.uiScaleIndex =
            std::clamp(appliedSettings_.interface.uiScaleIndex, 0, static_cast<int>(kUiScaleChoices.size()) - 1);
    }
} // namespace ui

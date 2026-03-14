#ifndef PHYSICS3D_PAUSEMENUSHARED_H
#define PHYSICS3D_PAUSEMENUSHARED_H

#include <array>
#include <cstddef>
#include <string>

#include "ui/OverlayRenderer.h"

namespace ui::pause_menu_shared {
    inline constexpr int kFontH = 7;
    inline constexpr int kAdvance = 6;
    inline constexpr float kBaseScalePx = 1.90f;
    inline constexpr float kSettingsScalePx = 2.00f;
    inline constexpr float kTitleScalePx = 2.85f;

    inline constexpr std::array<float, 7> kUiScaleChoices{{
        0.75f, 0.85f, 1.00f, 1.15f, 1.30f, 1.40f, 1.50f,
    }};

    inline constexpr std::array<double, 9> kMinSpeedChoices{{
        1.0 / 256.0, 1.0 / 128.0, 1.0 / 64.0, 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0,
    }};

    inline constexpr std::array<double, 9> kMaxSpeedChoices{{
        1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0,
    }};

    inline constexpr double kDefaultGravityStrength = 6.6743e-11;
    inline constexpr std::array<double, 9> kGravityStrengthChoices{{
        kDefaultGravityStrength * 0.125, kDefaultGravityStrength * 0.25, kDefaultGravityStrength * 0.50,
        kDefaultGravityStrength * 0.75,  kDefaultGravityStrength * 1.00, kDefaultGravityStrength * (4.0 / 3.0),
        kDefaultGravityStrength * 2.00,  kDefaultGravityStrength * 4.00, kDefaultGravityStrength * 8.00,
    }};

    inline constexpr std::array<int, 8> kCollisionIterationChoices{{1, 2, 3, 4, 5, 6, 8, 10}};
    inline constexpr std::array<double, 11> kGlobalRestitutionChoices{{
        0.00, 0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00,
    }};

    inline constexpr std::array<float, 9> kLookSensitivityChoices{{
        0.0008f, 0.0012f, 0.0016f, 0.0020f, 0.0025f, 0.0030f, 0.0038f, 0.0048f, 0.0060f,
    }};

    inline constexpr std::array<float, 7> kBaseMoveSpeedChoices{{
        10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f,
    }};

    inline constexpr std::array<float, 8> kFovChoices{{
        50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f,
    }};

    inline constexpr std::array<const char*, 5> kPageTabLabels{{
        "DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS",
    }};

    template <typename T, std::size_t N>
    int closestChoiceIndex(const std::array<T, N>& choices, const T value) {
        int bestIdx = 0;
        double bestDiff = static_cast<double>(choices[0] - value);
        bestDiff = bestDiff < 0.0 ? -bestDiff : bestDiff;
        for (std::size_t i = 1; i < N; ++i) {
            double diff = static_cast<double>(choices[i] - value);
            diff = diff < 0.0 ? -diff : diff;
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = static_cast<int>(i);
            }
        }
        return bestIdx;
    }

    struct Rect {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;

        [[nodiscard]] bool contains(const float x, const float y) const {
            return x >= x0 && x <= x1 && y >= y0 && y <= y1;
        }
    };

    struct PauseMenuLayout {
        float width = 0.0f;
        float height = 0.0f;
        float menuScale = 1.0f;
        float baseScalePx = 0.0f;
        float rowScalePx = 0.0f;
        float tabsScalePx = 0.0f;
        float sectionHeaderScalePx = 0.0f;
        float actionScalePx = 0.0f;
        float actionButtonH = 0.0f;
        float cardX0 = 0.0f;
        float cardY0 = 0.0f;
        float cardX1 = 0.0f;
        float cardY1 = 0.0f;
        float sectionX0 = 0.0f;
        float sectionX1 = 0.0f;
        float settingsY0 = 0.0f;
        float settingsY1 = 0.0f;
        float linesStartY = 0.0f;
        float rowStep = 0.0f;
        float contentY1 = 0.0f;
        std::size_t totalLines = 0;
        std::size_t visibleLines = 0;
        std::size_t firstLine = 0;
        std::array<Rect, kPageTabLabels.size()> tabRects{};
        Rect exitButton{};
        Rect closeButton{};
        Rect resetIcon{};
        Rect popupRect{};
        Rect applyAction{};
        Rect resetWorldAction{};
        Rect resetControlsAction{};

        [[nodiscard]] Rect lineRect(std::size_t visibleIndex) const;
    };

    [[nodiscard]] float measureMaxLinePx(const std::string& s, float scalePx);
    [[nodiscard]] float fitScaleForWidth(const std::string& s, float preferredScalePx, float maxWidthPx);
    [[nodiscard]] std::string formatSpeedMultiplier(double value);
    [[nodiscard]] std::string formatGravityMultiplier(double value);
    [[nodiscard]] std::string formatRestitutionPercent(double value);
    [[nodiscard]] PauseMenuLayout buildPauseMenuLayout(float width, float height, float uiScale, const OverlayRenderer::PauseMenuHud& hud);
} // namespace ui::pause_menu_shared

#endif // PHYSICS3D_PAUSEMENUSHARED_H

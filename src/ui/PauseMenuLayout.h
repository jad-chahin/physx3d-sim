#ifndef PHYSICS3D_UI_PAUSEMENULAYOUT_H
#define PHYSICS3D_UI_PAUSEMENULAYOUT_H

#include "ui/OverlayRenderer.h"

#include <array>
#include <cstddef>
#include <string>

namespace ui::pause_menu_layout {

inline constexpr int kFontH = 7;
inline constexpr int kAdvance = 6;
inline constexpr float kBaseScalePx = 1.90f;
inline constexpr float kSettingsScalePx = 2.00f;
inline constexpr float kTitleScalePx = 2.85f;

inline constexpr std::array<float, 7> kUiScaleChoices{{
    0.75f, 0.85f, 1.00f, 1.15f, 1.30f, 1.40f, 1.50f,
}};

inline constexpr double kDefaultGravityStrength = 6.6743e-11;

inline constexpr std::array<const char*, 5> kPageTabLabels{{
    "DISPLAY", "SIMULATION", "CAMERA", "INTERFACE", "CONTROLS",
}};

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

struct Rect {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    [[nodiscard]] bool contains(float x, float y) const {
        return x >= x0 && x <= x1 && y >= y0 && y <= y1;
    }
};

struct PauseMenuLayout {
    struct LineWidgets {
        Rect line{};
        Rect value{};
        Rect checkbox{};
        Rect leftArrow{};
        Rect rightArrow{};
        Rect slider{};
        bool hasValue = false;
        bool hasCheckbox = false;
        bool hasLeftArrow = false;
        bool hasRightArrow = false;
        bool hasSlider = false;
    };

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
    std::array<Rect, static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Count)> actionRects{};
    Rect popupRect{};

    [[nodiscard]] Rect lineRect(std::size_t visibleIndex) const;
    [[nodiscard]] const Rect& actionRect(OverlayRenderer::PauseMenuAction action) const;
    [[nodiscard]] LineWidgets lineWidgets(
        std::size_t visibleIndex,
        const OverlayRenderer::PauseMenuHudLine& line) const;
};

struct LayoutConfig {
    float minUiScale = 0.75f;
    float maxUiScale = 1.50f;
    float actionScaleFactor = 0.98f;
    float statusScaleFactor = 0.95f;
    float footerScaleFactor = 0.92f;
    float sectionHeaderScaleFactor = 1.18f;
    float rowScaleFactor = 1.8f;
    float inlineValueScaleFactor = 0.88f;
    float cardWidthRatio = 0.82f;
    float cardHeightRatio = 0.86f;
    float cardMaxWidthPx = 1240.0f;
    float cardMaxHeightPx = 920.0f;
    float topActionInsetPx = 18.0f;
    float sectionInsetPx = 26.0f;
    float settingsTopMinPx = 170.0f;
    float sectionGapPx = 16.0f;
    float settingsBottomInsetPx = 20.0f;
    float footerReservedInsetPx = 18.0f;
    float minSettingsHeightPx = 220.0f;
    float statusYOffsetPx = 96.0f;
    float statusGapPx = 16.0f;
    float tabPadXPx = 12.0f;
    float tabPadYPx = 7.0f;
    float tabGapPx = 12.0f;
    float sectionHeaderOffsetXPx = 14.0f;
    float sectionHeaderOffsetYPx = 10.0f;
    float linesGapPx = 9.0f;
    float lineInsetXPx = 10.0f;
    float lineOffsetYPx = 2.0f;
    float lineHeightExtraPx = 3.5f;
    float scrollbarReservePx = 22.0f;
    float scrollbarInsetPx = 8.0f;
    float scrollbarMinThumbPx = 18.0f;
    float valueInsetRightPx = 10.0f;
    float toggleValueWidthPx = 220.0f;
    float inlineArrowWidthPx = 24.0f;
    float inlineGapPx = 6.0f;
    float toggleCheckboxInsetXPx = 18.0f;
    float toggleCheckboxInsetYPx = 8.0f;
    float minToggleCheckboxPx = 8.0f;
    float sliderHeightPx = 6.0f;
    float sliderInsetXPx = 6.0f;
    float sliderBottomInsetPx = 3.0f;
    float rebindMinWidthPx = 88.0f;
    float rebindMaxWidthPx = 150.0f;
    float rebindPadXPx = 18.0f;
    float plainValueInsetXPx = 2.0f;
    float rowAreaInsetPx = 28.0f;
    float actionPadXPx = 16.0f;
    float actionPadYPx = 8.0f;
    float actionGapPx = 12.0f;
    float actionBottomInsetPx = 10.0f;
    float actionReservedGapPx = 20.0f;
    float popupWidthPx = 320.0f;
    float popupHeightPx = 120.0f;
    float popupButtonBottomInsetPx = 14.0f;
    float popupButtonSideInsetPx = 18.0f;
    float popupButtonGapPx = 10.0f;
    float toggleControlReservePx = 128.0f;
    float choiceControlReservePx = 248.0f;
    float rebindControlReservePx = 240.0f;
    float actionControlReservePx = 120.0f;
};

struct PopupButtonsLayout {
    bool singleAcknowledge = false;
    float buttonScalePx = 0.0f;
    float buttonPadX = 0.0f;
    float buttonPadY = 0.0f;
    Rect yesButton{};
    Rect noButton{};
    std::string yesLabel;
    std::string noLabel;
};

[[nodiscard]] std::string formatSpeedMultiplier(double value);
[[nodiscard]] std::string formatGravityMultiplier(double value);
[[nodiscard]] std::string formatRestitutionPercent(double value);
[[nodiscard]] const LayoutConfig& layoutConfig();
[[nodiscard]] PauseMenuLayout buildPauseMenuLayout(
    float width,
    float height,
    float uiScale,
    const OverlayRenderer::PauseMenuHud& hud);
[[nodiscard]] PopupButtonsLayout buildPopupButtonsLayout(
    const PauseMenuLayout& layout,
    const OverlayRenderer::PauseMenuHud& hud);

} // namespace ui::pause_menu_layout

#endif // PHYSICS3D_UI_PAUSEMENULAYOUT_H

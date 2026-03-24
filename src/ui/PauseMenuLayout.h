#ifndef PHYSICS3D_UI_PAUSEMENULAYOUT_H
#define PHYSICS3D_UI_PAUSEMENULAYOUT_H

#include "ui/PauseMenu.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ui::pause_menu_layout {

inline constexpr int kFontH = 7;
inline constexpr int kAdvance = 6;
inline constexpr float kBaseScalePx = 1.90f;
inline constexpr float kTitleScalePx = 2.85f;

struct Rect {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    [[nodiscard]] bool contains(float x, float y) const {
        return x >= x0 && x <= x1 && y >= y0 && y <= y1;
    }
};

struct LineWidgets {
    Rect line{};
    Rect value{};
    Rect checkbox{};
    Rect leftArrow{};
    Rect rightArrow{};
};

struct PauseMenuLayout {
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
    std::vector<Rect> tabRects;
    std::vector<Rect> actionRects;
    Rect popupRect{};

    [[nodiscard]] Rect lineRect(std::size_t visibleIndex) const;
    [[nodiscard]] const Rect& actionRect(std::size_t actionIndex) const;
    [[nodiscard]] LineWidgets lineWidgets(std::size_t visibleIndex, const ViewRow& line) const;
};

struct PopupButtonsLayout {
    float buttonScalePx = 0.0f;
    float buttonPadX = 0.0f;
    float buttonPadY = 0.0f;
    Rect yesButton{};
    Rect noButton{};
    std::string yesLabel;
    std::string noLabel;
};

[[nodiscard]] PauseMenuLayout buildPauseMenuLayout(
    float width,
    float height,
    float uiScale,
    const MenuView& view);
[[nodiscard]] PopupButtonsLayout buildPopupButtonsLayout(
    const PauseMenuLayout& layout,
    const ViewPopup& popup);

} // namespace ui::pause_menu_layout

#endif // PHYSICS3D_UI_PAUSEMENULAYOUT_H

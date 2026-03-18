#include "ui/PauseMenuLayout.h"

#include <algorithm>
#include <cmath>

namespace ui::pause_menu_layout {
namespace {

constexpr float kMinUiScale = 0.75f;
constexpr float kMaxUiScale = 1.50f;
constexpr float kSettingsScalePx = 2.00f;
constexpr float kCardWidthRatio = 0.82f;
constexpr float kCardHeightRatio = 0.86f;
constexpr float kCardMaxWidthPx = 1240.0f;
constexpr float kCardMaxHeightPx = 920.0f;

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
    return static_cast<float>(std::max(maxChars, lineChars)) * scalePx;
}

float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
    const float unitWidth = measureMaxLinePx(s, 1.0f);
    return unitWidth <= 0.0f || maxWidthPx <= 0.0f ? preferredScalePx : std::min(preferredScalePx, maxWidthPx / unitWidth);
}

int selectedRowIndex(const MenuView& view) {
    for (std::size_t i = 0; i < view.rows.size(); ++i) {
        if (view.rows[i].selected) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<std::size_t> actionIndexes(const MenuView& view, const ActionPlacement placement) {
    std::vector<std::size_t> indexes;
    for (std::size_t i = 0; i < view.actions.size(); ++i) {
        if (view.actions[i].placement == placement) {
            indexes.push_back(i);
        }
    }
    return indexes;
}

std::size_t findActionIndex(const MenuView& view, const ActionKind kind) {
    for (std::size_t i = 0; i < view.actions.size(); ++i) {
        if (view.actions[i].id == static_cast<int>(kind)) {
            return i;
        }
    }
    return view.actions.size();
}

} // namespace

Rect PauseMenuLayout::lineRect(const std::size_t visibleIndex) const {
    const float lineY = linesStartY + static_cast<float>(visibleIndex) * rowStep;
    const float scrollbarReserve = totalLines > visibleLines ? 22.0f * menuScale : 0.0f;
    return {
        sectionX0 + 10.0f * menuScale,
        lineY - 2.0f * menuScale,
        sectionX1 - 10.0f * menuScale - scrollbarReserve,
        lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale,
    };
}

const Rect& PauseMenuLayout::actionRect(const std::size_t actionIndex) const {
    return actionRects[actionIndex];
}

LineWidgets PauseMenuLayout::lineWidgets(const std::size_t visibleIndex, const ViewRow& line) const {
    LineWidgets widgets{};
    widgets.line = lineRect(visibleIndex);
    if (line.kind == RowKind::Header) {
        return widgets;
    }

    const float y0 = widgets.line.y0;
    const float y1 = widgets.line.y1;
    const float rightX1 = widgets.line.x1 - 10.0f * menuScale;
    const float valueScalePx = rowScalePx * 0.88f;

    if (line.kind == RowKind::Toggle) {
        const float valueX1 = rightX1 - 24.0f * menuScale - 6.0f * menuScale;
        widgets.value = {valueX1 - 220.0f * menuScale, y0, valueX1, y1};
        const float box = std::max(8.0f, (y1 - y0) - 8.0f * menuScale);
        const float boxY0 = y0 + ((y1 - y0) - box) * 0.5f;
        widgets.checkbox = {widgets.value.x0 + 18.0f * menuScale, boxY0, widgets.value.x0 + 18.0f * menuScale + box, boxY0 + box};
        return widgets;
    }

    if (line.kind == RowKind::Choice) {
        widgets.rightArrow = {rightX1 - 24.0f * menuScale, y0, rightX1, y1};
        widgets.value = {widgets.rightArrow.x0 - 6.0f * menuScale - 220.0f * menuScale, y0, widgets.rightArrow.x0 - 6.0f * menuScale, y1};
        widgets.leftArrow = {widgets.value.x0 - 6.0f * menuScale - 24.0f * menuScale, y0, widgets.value.x0 - 6.0f * menuScale, y1};
        return widgets;
    }

    if (line.kind == RowKind::Rebind && !line.value.empty()) {
        const float valueW = std::clamp(
            measureMaxLinePx(line.value, valueScalePx) + 18.0f * menuScale,
            88.0f * menuScale,
            150.0f * menuScale);
        widgets.value = {rightX1 - valueW, y0, rightX1, y1};
    }
    return widgets;
}

PauseMenuLayout buildPauseMenuLayout(
    const float width,
    const float height,
    const float uiScale,
    const MenuView& view)
{
    PauseMenuLayout layout{};
    layout.menuScale = std::clamp(uiScale, kMinUiScale, kMaxUiScale);
    layout.baseScalePx = kBaseScalePx * layout.menuScale;
    layout.tabsScalePx = layout.baseScalePx;
    layout.actionScalePx = layout.baseScalePx * 0.98f;

    const float cardWidth = std::min(width * kCardWidthRatio, kCardMaxWidthPx);
    const float cardHeight = std::min(height * kCardHeightRatio, kCardMaxHeightPx);
    layout.cardX0 = (width - cardWidth) * 0.5f;
    layout.cardY0 = (height - cardHeight) * 0.5f;
    layout.cardX1 = layout.cardX0 + cardWidth;
    layout.cardY1 = layout.cardY0 + cardHeight;

    const float statusY = layout.cardY0 + 96.0f * layout.menuScale;
    const float statusHeight = (static_cast<float>(kFontH) + 2.0f) * (layout.baseScalePx * 0.95f);
    const float tabsY = statusY + statusHeight + 16.0f * layout.menuScale;
    const float tabPadX = 12.0f * layout.menuScale;
    const float tabPadY = 7.0f * layout.menuScale;
    const float tabGap = 12.0f * layout.menuScale;
    const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * layout.tabsScalePx + tabPadY * 2.0f;
    const float topInset = 18.0f * layout.menuScale;

    float tabX = layout.cardX0 + 18.0f * layout.menuScale;
    for (const auto& tab : view.tabs) {
        const float tabWidth = measureMaxLinePx(tab, layout.tabsScalePx) + tabPadX * 2.0f;
        layout.tabRects.push_back({tabX, tabsY, tabX + tabWidth, tabsY + tabHeight});
        tabX += tabWidth + tabGap;
    }

    layout.actionRects.assign(view.actions.size(), {});
    const auto layoutActions = [&](const ActionPlacement placement,
                                   const float padX,
                                   const float y0,
                                   const float y1,
                                   const float rightInset,
                                   const float gap,
                                   const float scalePx) {
        const auto indexes = actionIndexes(view, placement);
        float totalWidth = 0.0f;
        for (std::size_t i = 0; i < indexes.size(); ++i) {
            totalWidth += measureMaxLinePx(view.actions[indexes[i]].label, scalePx) + padX * 2.0f;
            if (i + 1 < indexes.size()) {
                totalWidth += gap;
            }
        }

        float x = rightInset - totalWidth;
        for (const std::size_t index : indexes) {
            const float widthPx = measureMaxLinePx(view.actions[index].label, scalePx) + padX * 2.0f;
            layout.actionRects[index] = {x, y0, x + widthPx, y1};
            x += widthPx + gap;
        }
    };

    const std::size_t resetIndex = findActionIndex(view, ActionKind::ResetSettings);
    if (resetIndex < view.actions.size()) {
        const float resetWidth = measureMaxLinePx(view.actions[resetIndex].label, layout.tabsScalePx) + tabPadX * 2.0f;
        layout.actionRects[resetIndex] = {
            layout.cardX0 + topInset,
            layout.cardY0 + topInset,
            layout.cardX0 + topInset + resetWidth,
            layout.cardY0 + topInset + tabHeight,
        };
    }

    const std::size_t closeIndex = findActionIndex(view, ActionKind::Close);
    if (closeIndex < view.actions.size()) {
        const float closeWidth = measureMaxLinePx(view.actions[closeIndex].label, layout.tabsScalePx) + tabPadX * 2.0f;
        layout.actionRects[closeIndex] = {
            layout.cardX1 - topInset - closeWidth,
            layout.cardY0 + topInset,
            layout.cardX1 - topInset,
            layout.cardY0 + topInset + tabHeight,
        };
    }

    const std::size_t exitIndex = findActionIndex(view, ActionKind::Exit);
    if (exitIndex < view.actions.size()) {
        const float exitWidth = measureMaxLinePx(view.actions[exitIndex].label, layout.tabsScalePx) + tabPadX * 2.0f;
        layout.actionRects[exitIndex] = {
            layout.cardX1 - topInset - exitWidth,
            tabsY,
            layout.cardX1 - topInset,
            tabsY + tabHeight,
        };
    }

    const float footerReservedH =
        (static_cast<float>(kFontH) + 3.0f) * (layout.baseScalePx * 0.92f) +
        18.0f * layout.menuScale;
    layout.settingsY0 = std::max(
        layout.cardY0 + 170.0f * layout.menuScale,
        tabsY + (view.tabs.empty() ? 0.0f : tabHeight) + 16.0f * layout.menuScale);
    layout.sectionX0 = layout.cardX0 + 26.0f * layout.menuScale;
    layout.sectionX1 = layout.cardX1 - 26.0f * layout.menuScale;
    layout.settingsY1 = layout.cardY1 - 20.0f * layout.menuScale - footerReservedH;
    if (layout.settingsY1 - layout.settingsY0 < 220.0f) {
        layout.settingsY0 = layout.settingsY1 - 220.0f;
    }

    float maxLabelWidth = 0.0f;
    float maxControlReserve = 0.0f;
    for (const auto& row : view.rows) {
        maxLabelWidth = std::max(maxLabelWidth, measureMaxLinePx(row.label, 1.0f));
        if (row.kind == RowKind::Toggle) {
            maxControlReserve = std::max(maxControlReserve, 128.0f * layout.menuScale);
        } else if (row.kind == RowKind::Choice) {
            maxControlReserve = std::max(maxControlReserve, 248.0f * layout.menuScale);
        } else if (row.kind == RowKind::Rebind) {
            maxControlReserve = std::max(maxControlReserve, 240.0f * layout.menuScale);
        }
    }

    layout.rowScalePx = kSettingsScalePx * layout.menuScale * 1.8f;
    const float rowAreaWidth =
        (layout.sectionX1 - layout.sectionX0) - 28.0f * layout.menuScale - maxControlReserve;
    if (maxLabelWidth > 0.0f && rowAreaWidth > 0.0f) {
        layout.rowScalePx = std::min(layout.rowScalePx, rowAreaWidth / maxLabelWidth);
    }

    const std::string sectionTitle = view.sectionTitle.empty() ? "SETTINGS" : view.sectionTitle;
    layout.sectionHeaderScalePx = fitScaleForWidth(
        sectionTitle,
        layout.rowScalePx * 1.18f,
        (layout.sectionX1 - layout.sectionX0) - 28.0f * layout.menuScale);
    layout.linesStartY =
        layout.settingsY0 + 10.0f * layout.menuScale +
        (static_cast<float>(kFontH) + 3.0f) * layout.sectionHeaderScalePx +
        9.0f * layout.menuScale;
    layout.rowStep = (static_cast<float>(kFontH) + 4.0f) * layout.rowScalePx;
    layout.actionButtonH =
        (static_cast<float>(kFontH) + 2.0f) * layout.actionScalePx +
        16.0f * layout.menuScale;
    layout.contentY1 = layout.settingsY1 - 10.0f * layout.menuScale;
    if (!actionIndexes(view, ActionPlacement::Bottom).empty()) {
        layout.contentY1 -= layout.actionButtonH + 20.0f * layout.menuScale;
    }

    layout.totalLines = view.rows.size();
    layout.visibleLines = std::min<std::size_t>(
        layout.totalLines,
        static_cast<std::size_t>(std::max(0.0f, std::floor((layout.contentY1 - layout.linesStartY) / layout.rowStep))));
    if (layout.visibleLines > 0 && layout.totalLines > layout.visibleLines) {
        const std::size_t maxFirst = layout.totalLines - layout.visibleLines;
        layout.firstLine = std::min<std::size_t>(
            static_cast<std::size_t>(std::max(0, view.firstVisibleLineIndex)),
            maxFirst);
        if (view.keepSelectedVisible) {
            const int selected = selectedRowIndex(view);
            if (selected >= 0) {
                const auto selectedLine = static_cast<std::size_t>(selected);
                if (selectedLine < layout.firstLine) {
                    layout.firstLine = selectedLine;
                } else if (selectedLine >= layout.firstLine + layout.visibleLines) {
                    layout.firstLine = selectedLine - layout.visibleLines + 1;
                }
            }
            layout.firstLine = std::min(layout.firstLine, maxFirst);
        }
    }

    layout.popupRect = {
        layout.cardX0 + ((layout.cardX1 - layout.cardX0) - 320.0f * layout.menuScale) * 0.5f,
        layout.cardY0 + ((layout.cardY1 - layout.cardY0) - 120.0f * layout.menuScale) * 0.5f,
        0.0f,
        0.0f,
    };
    layout.popupRect.x1 = layout.popupRect.x0 + 320.0f * layout.menuScale;
    layout.popupRect.y1 = layout.popupRect.y0 + 120.0f * layout.menuScale;

    const float bottomY1 = layout.settingsY1 - 10.0f * layout.menuScale;
    layoutActions(
        ActionPlacement::Bottom,
        16.0f * layout.menuScale,
        bottomY1 - layout.actionButtonH,
        bottomY1,
        layout.sectionX1 - 12.0f * layout.menuScale,
        12.0f * layout.menuScale,
        layout.actionScalePx);

    return layout;
}

PopupButtonsLayout buildPopupButtonsLayout(const PauseMenuLayout& layout, const ViewPopup& popup) {
    PopupButtonsLayout buttons{};
    buttons.singleAcknowledge = popup.singleAction;
    buttons.buttonScalePx = layout.baseScalePx * 0.92f;
    buttons.buttonPadX = 16.0f * layout.menuScale;
    buttons.buttonPadY = 8.0f * layout.menuScale;
    buttons.yesLabel = popup.confirmLabel.empty() ? "RESET" : popup.confirmLabel;
    buttons.noLabel = popup.cancelLabel.empty() ? "CANCEL" : popup.cancelLabel;

    const float yesWidth = measureMaxLinePx(buttons.yesLabel, buttons.buttonScalePx) + buttons.buttonPadX * 2.0f;
    const float buttonHeight =
        (static_cast<float>(kFontH) + 2.0f) * buttons.buttonScalePx + buttons.buttonPadY * 2.0f;
    const float y1 = layout.popupRect.y1 - 14.0f * layout.menuScale;
    const float y0 = y1 - buttonHeight;

    if (buttons.singleAcknowledge) {
        buttons.yesButton = {
            layout.popupRect.x0 + ((layout.popupRect.x1 - layout.popupRect.x0) - yesWidth) * 0.5f,
            y0,
            layout.popupRect.x0 + ((layout.popupRect.x1 - layout.popupRect.x0) - yesWidth) * 0.5f + yesWidth,
            y1,
        };
        return buttons;
    }

    const float noWidth = measureMaxLinePx(buttons.noLabel, buttons.buttonScalePx) + buttons.buttonPadX * 2.0f;
    buttons.noButton = {
        layout.popupRect.x1 - 18.0f * layout.menuScale - noWidth,
        y0,
        layout.popupRect.x1 - 18.0f * layout.menuScale,
        y1,
    };
    buttons.yesButton = {
        buttons.noButton.x0 - 10.0f * layout.menuScale - yesWidth,
        y0,
        buttons.noButton.x0 - 10.0f * layout.menuScale,
        y1,
    };
    return buttons;
}

} // namespace ui::pause_menu_layout

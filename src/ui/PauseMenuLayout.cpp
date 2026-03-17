#include "ui/PauseMenuLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui::pause_menu_layout {
namespace {

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
    if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) {
        return preferredScalePx;
    }
    return std::min(preferredScalePx, maxWidthPx / unitWidth);
}

} // namespace

const LayoutConfig& layoutConfig() {
    static constexpr LayoutConfig config{};
    return config;
}

Rect PauseMenuLayout::lineRect(const std::size_t visibleIndex) const {
    const auto& c = layoutConfig();
    const float lineY = linesStartY + static_cast<float>(visibleIndex) * rowStep;
    const float scrollbarReserve = totalLines > visibleLines ? c.scrollbarReservePx * menuScale : 0.0f;
    return {
        .x0 = sectionX0 + c.lineInsetXPx * menuScale,
        .y0 = lineY - c.lineOffsetYPx * menuScale,
        .x1 = sectionX1 - c.lineInsetXPx * menuScale - scrollbarReserve,
        .y1 = lineY + (static_cast<float>(kFontH) + c.lineHeightExtraPx) * rowScalePx + c.lineOffsetYPx * menuScale,
    };
}

const Rect& PauseMenuLayout::actionRect(const OverlayRenderer::PauseMenuAction action) const {
    return actionRects[static_cast<std::size_t>(action)];
}

PauseMenuLayout::LineWidgets PauseMenuLayout::lineWidgets(
    const std::size_t visibleIndex,
    const OverlayRenderer::PauseMenuHudLine& line) const
{
    const auto& c = layoutConfig();
    LineWidgets w{};
    w.line = lineRect(visibleIndex);
    if (line.header) {
        return w;
    }

    const float y0 = w.line.y0;
    const float y1 = w.line.y1;
    const float rightX1 = w.line.x1 - c.valueInsetRightPx * menuScale;
    const float valueScalePx = rowScalePx * c.inlineValueScaleFactor;

    switch (line.controlType) {
        case OverlayRenderer::PauseMenuControlType::Toggle: {
            const float valueW = c.toggleValueWidthPx * menuScale;
            const float valueX1 = rightX1 - c.inlineArrowWidthPx * menuScale - c.inlineGapPx * menuScale;
            w.value = {valueX1 - valueW, y0, valueX1, y1};
            w.hasValue = true;

            const float box = std::max(
                c.minToggleCheckboxPx,
                (y1 - y0) - c.toggleCheckboxInsetYPx * menuScale);
            const float x0 = w.value.x0 + c.toggleCheckboxInsetXPx * menuScale;
            const float boxY0 = y0 + ((y1 - y0) - box) * 0.5f;
            w.checkbox = {x0, boxY0, x0 + box, boxY0 + box};
            w.hasCheckbox = true;
            break;
        }
        case OverlayRenderer::PauseMenuControlType::Choice:
        case OverlayRenderer::PauseMenuControlType::Numeric: {
            const float arrowW = c.inlineArrowWidthPx * menuScale;
            const float gap = c.inlineGapPx * menuScale;
            const float valueW = c.toggleValueWidthPx * menuScale;
            w.rightArrow = {rightX1 - arrowW, y0, rightX1, y1};
            w.hasRightArrow = true;
            w.value = {w.rightArrow.x0 - gap - valueW, y0, w.rightArrow.x0 - gap, y1};
            w.hasValue = true;
            w.leftArrow = {w.value.x0 - gap - arrowW, y0, w.value.x0 - gap, y1};
            w.hasLeftArrow = true;
            if (line.controlType == OverlayRenderer::PauseMenuControlType::Numeric && line.showSlider) {
                const float sliderY1 = y1 - c.sliderBottomInsetPx * menuScale;
                w.slider = {
                    w.value.x0 + c.sliderInsetXPx * menuScale,
                    sliderY1 - c.sliderHeightPx * menuScale,
                    w.value.x1 - c.sliderInsetXPx * menuScale,
                    sliderY1,
                };
                w.hasSlider = true;
            }
            break;
        }
        case OverlayRenderer::PauseMenuControlType::Rebind:
        case OverlayRenderer::PauseMenuControlType::Action:
            if (!line.valueText.empty()) {
                const float valueW = std::clamp(
                    measureMaxLinePx(line.valueText, valueScalePx) + c.rebindPadXPx * menuScale,
                    c.rebindMinWidthPx * menuScale,
                    c.rebindMaxWidthPx * menuScale);
                w.value = {rightX1 - valueW, y0, rightX1, y1};
                w.hasValue = true;
            }
            break;
        case OverlayRenderer::PauseMenuControlType::None:
        default:
            if (!line.valueText.empty()) {
                const float valueW = measureMaxLinePx(line.valueText, valueScalePx);
                w.value = {
                    rightX1 - c.plainValueInsetXPx * menuScale - valueW,
                    y0,
                    rightX1 - c.plainValueInsetXPx * menuScale,
                    y1,
                };
                w.hasValue = true;
            }
            break;
    }

    return w;
}

std::string formatSpeedMultiplier(const double value) {
    char b[32];
    if (value >= 100.0) {
        std::snprintf(b, sizeof(b), "%.0fX", value);
    } else if (value >= 10.0) {
        std::snprintf(b, sizeof(b), "%.1fX", value);
    } else if (value >= 1.0) {
        std::snprintf(b, sizeof(b), "%.2fX", value);
    } else {
        std::snprintf(b, sizeof(b), "%.4fX", value);
    }
    return b;
}

std::string formatGravityMultiplier(const double value) {
    char b[32];
    const double m = value / kDefaultGravityStrength;
    if (m >= 10.0) {
        std::snprintf(b, sizeof(b), "%.0fX", m);
    } else {
        std::snprintf(b, sizeof(b), "%.2fX", m);
    }
    return b;
}

std::string formatRestitutionPercent(const double value) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.0f%%", std::clamp(value, 0.0, 1.0) * 100.0);
    return b;
}

PauseMenuLayout buildPauseMenuLayout(
    const float width,
    const float height,
    const float uiScale,
    const OverlayRenderer::PauseMenuHud& hud)
{
    const auto& c = layoutConfig();
    PauseMenuLayout l{};
    l.width = width;
    l.height = height;
    l.menuScale = std::clamp(uiScale, c.minUiScale, c.maxUiScale);
    l.baseScalePx = kBaseScalePx * l.menuScale;
    l.tabsScalePx = l.baseScalePx;
    l.actionScalePx = l.baseScalePx * c.actionScaleFactor;

    const float cardWidth = std::min(width * c.cardWidthRatio, c.cardMaxWidthPx);
    const float cardHeight = std::min(height * c.cardHeightRatio, c.cardMaxHeightPx);
    l.cardX0 = (width - cardWidth) * 0.5f;
    l.cardY0 = (height - cardHeight) * 0.5f;
    l.cardX1 = l.cardX0 + cardWidth;
    l.cardY1 = l.cardY0 + cardHeight;

    const float statusSlotY = l.cardY0 + c.statusYOffsetPx * l.menuScale;
    const float statusReservedScalePx = l.baseScalePx * c.statusScaleFactor;
    const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
    const float tabsSlotY = statusSlotY + statusReservedHeight + c.statusGapPx * l.menuScale;
    const float tabPadX = c.tabPadXPx * l.menuScale;
    const float tabPadY = c.tabPadYPx * l.menuScale;
    const float tabGap = c.tabGapPx * l.menuScale;
    const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * l.tabsScalePx + tabPadY * 2.0f;

    float tabX = l.cardX0 + c.topActionInsetPx * l.menuScale;
    for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
        const float tabWidth = measureMaxLinePx(kPageTabLabels[i], l.tabsScalePx) + tabPadX * 2.0f;
        l.tabRects[i] = {tabX, tabsSlotY, tabX + tabWidth, tabsSlotY + tabHeight};
        tabX += tabWidth + tabGap;
    }

    const float buttonPadX = c.tabPadXPx * l.menuScale;
    const float exitWidth = measureMaxLinePx("EXIT TO HOME", l.tabsScalePx) + buttonPadX * 2.0f;
    const float closeWidth = measureMaxLinePx("X", l.tabsScalePx) + buttonPadX * 2.0f;
    const float resetWidth = measureMaxLinePx("R", l.tabsScalePx) + buttonPadX * 2.0f;

    l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Exit)] = {
        l.cardX1 - c.topActionInsetPx * l.menuScale - exitWidth,
        tabsSlotY,
        l.cardX1 - c.topActionInsetPx * l.menuScale,
        tabsSlotY + tabHeight,
    };
    l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Close)] = {
        l.cardX1 - c.topActionInsetPx * l.menuScale - closeWidth,
        l.cardY0 + c.topActionInsetPx * l.menuScale,
        l.cardX1 - c.topActionInsetPx * l.menuScale,
        l.cardY0 + c.topActionInsetPx * l.menuScale + tabHeight,
    };
    l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::ResetIcon)] = {
        l.cardX0 + c.topActionInsetPx * l.menuScale,
        l.cardY0 + c.topActionInsetPx * l.menuScale,
        l.cardX0 + c.topActionInsetPx * l.menuScale + resetWidth,
        l.cardY0 + c.topActionInsetPx * l.menuScale + tabHeight,
    };

    const float footerReservedH =
        (static_cast<float>(kFontH) + 3.0f) * (l.baseScalePx * c.footerScaleFactor) +
        c.footerReservedInsetPx * l.menuScale;
    l.settingsY0 = std::max(
        l.cardY0 + c.settingsTopMinPx * l.menuScale,
        tabsSlotY + tabHeight + c.sectionGapPx * l.menuScale);
    l.sectionX0 = l.cardX0 + c.sectionInsetPx * l.menuScale;
    l.sectionX1 = l.cardX1 - c.sectionInsetPx * l.menuScale;
    l.settingsY1 = l.cardY1 - c.settingsBottomInsetPx * l.menuScale - footerReservedH;
    if (l.settingsY1 - l.settingsY0 < c.minSettingsHeightPx) {
        l.settingsY0 = l.settingsY1 - c.minSettingsHeightPx;
    }

    float maxRowWidth = 0.0f;
    float maxControlReserve = 0.0f;
    for (const auto& line : hud.lines) {
        maxRowWidth = std::max(maxRowWidth, measureMaxLinePx(line.text, 1.0f));
        if (line.header) {
            continue;
        }
        switch (line.controlType) {
            case OverlayRenderer::PauseMenuControlType::Toggle:
                maxControlReserve = std::max(maxControlReserve, c.toggleControlReservePx * l.menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Choice:
            case OverlayRenderer::PauseMenuControlType::Numeric:
                maxControlReserve = std::max(maxControlReserve, c.choiceControlReservePx * l.menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Rebind:
                maxControlReserve = std::max(maxControlReserve, c.rebindControlReservePx * l.menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::Action:
                maxControlReserve = std::max(maxControlReserve, c.actionControlReservePx * l.menuScale);
                break;
            case OverlayRenderer::PauseMenuControlType::None:
                break;
        }
    }

    l.rowScalePx = kSettingsScalePx * l.menuScale * c.rowScaleFactor;
    if (const float rowAreaWidth =
            (l.sectionX1 - l.sectionX0) - c.rowAreaInsetPx * l.menuScale - maxControlReserve;
        maxRowWidth > 0.0f && rowAreaWidth > 0.0f)
    {
        l.rowScalePx = std::min(l.rowScalePx, rowAreaWidth / maxRowWidth);
    }

    l.sectionHeaderScalePx = fitScaleForWidth(
        "SETTINGS",
        l.rowScalePx * c.sectionHeaderScaleFactor,
        (l.sectionX1 - l.sectionX0) - c.rowAreaInsetPx);
    l.linesStartY =
        l.settingsY0 + c.sectionHeaderOffsetYPx * l.menuScale +
        (static_cast<float>(kFontH) + 3.0f) * l.sectionHeaderScalePx +
        c.linesGapPx * l.menuScale;
    l.rowStep = (static_cast<float>(kFontH) + 4.0f) * l.rowScalePx;
    l.actionButtonH =
        (static_cast<float>(kFontH) + 2.0f) * l.actionScalePx +
        c.actionPadYPx * l.menuScale * 2.0f;
    l.contentY1 =
        l.settingsY1 - c.actionBottomInsetPx * l.menuScale -
        (l.actionButtonH + c.actionReservedGapPx * l.menuScale);

    l.totalLines = hud.lines.size();
    l.visibleLines = std::min<std::size_t>(
        l.totalLines,
        static_cast<std::size_t>(std::max(0.0f, std::floor((l.contentY1 - l.linesStartY) / l.rowStep))));
    l.firstLine = 0;

    if (l.visibleLines > 0 && l.totalLines > l.visibleLines) {
        const std::size_t maxFirst = l.totalLines - l.visibleLines;
        l.firstLine = std::min<std::size_t>(
            static_cast<std::size_t>(std::max(0, hud.firstVisibleLineIndex)),
            maxFirst);
        if (hud.selectedSettingLineIndex >= 0) {
            const auto selected = static_cast<std::size_t>(std::clamp(
                hud.selectedSettingLineIndex,
                0,
                static_cast<int>(l.totalLines - 1)));
            if (selected < l.firstLine) {
                l.firstLine = selected;
            } else if (selected >= l.firstLine + l.visibleLines) {
                l.firstLine = selected - l.visibleLines + 1;
            }
        }
        l.firstLine = std::min(l.firstLine, maxFirst);
    }

    l.popupRect = {
        l.cardX0 + ((l.cardX1 - l.cardX0) - c.popupWidthPx * l.menuScale) * 0.5f,
        l.cardY0 + ((l.cardY1 - l.cardY0) - c.popupHeightPx * l.menuScale) * 0.5f,
        0.0f,
        0.0f,
    };
    l.popupRect.x1 = l.popupRect.x0 + c.popupWidthPx * l.menuScale;
    l.popupRect.y1 = l.popupRect.y0 + c.popupHeightPx * l.menuScale;

    const float actionPadX = c.actionPadXPx * l.menuScale;
    if (hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Apply)].visible) {
        const float w = measureMaxLinePx("APPLY CHANGES", l.actionScalePx) + actionPadX * 2.0f;
        l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Apply)] = {
            l.sectionX1 - c.actionGapPx * l.menuScale - w,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale - l.actionButtonH,
            l.sectionX1 - c.actionGapPx * l.menuScale,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale,
        };
    }
    if (hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::ResetWorld)].visible) {
        const float w = measureMaxLinePx("RESET WORLD", l.actionScalePx) + actionPadX * 2.0f;
        const float applyW =
            hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::Apply)].visible
                ? measureMaxLinePx("APPLY CHANGES", l.actionScalePx) + actionPadX * 2.0f +
                      c.actionGapPx * l.menuScale
                : 0.0f;
        const float x1 = l.sectionX1 - c.actionGapPx * l.menuScale - applyW;
        l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::ResetWorld)] = {
            x1 - w,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale - l.actionButtonH,
            x1,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale,
        };
    }
    if (hud.actions[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::ResetControls)].visible) {
        const float w = measureMaxLinePx("RESET CONTROLS", l.actionScalePx) + actionPadX * 2.0f;
        l.actionRects[static_cast<std::size_t>(OverlayRenderer::PauseMenuAction::ResetControls)] = {
            l.sectionX1 - c.actionGapPx * l.menuScale - w,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale - l.actionButtonH,
            l.sectionX1 - c.actionGapPx * l.menuScale,
            l.settingsY1 - c.actionBottomInsetPx * l.menuScale,
        };
    }

    return l;
}

PopupButtonsLayout buildPopupButtonsLayout(
    const PauseMenuLayout& layout,
    const OverlayRenderer::PauseMenuHud& hud)
{
    const auto& c = layoutConfig();
    PopupButtonsLayout p{};
    p.singleAcknowledge = hud.resetConfirmSingleAcknowledge;
    p.buttonScalePx = layout.baseScalePx * c.footerScaleFactor;
    p.buttonPadX = c.actionPadXPx * layout.menuScale;
    p.buttonPadY = c.actionPadYPx * layout.menuScale;
    p.yesLabel = hud.resetConfirmYesLabel.empty() ? "RESET" : hud.resetConfirmYesLabel;
    p.noLabel = hud.resetConfirmNoLabel.empty() ? "CANCEL" : hud.resetConfirmNoLabel;

    const float yesWidth = measureMaxLinePx(p.yesLabel, p.buttonScalePx) + p.buttonPadX * 2.0f;
    const float buttonHeight =
        (static_cast<float>(kFontH) + 2.0f) * p.buttonScalePx + p.buttonPadY * 2.0f;
    const float y1 = layout.popupRect.y1 - c.popupButtonBottomInsetPx * layout.menuScale;
    const float y0 = y1 - buttonHeight;

    if (p.singleAcknowledge) {
        p.yesButton = {
            layout.popupRect.x0 + ((layout.popupRect.x1 - layout.popupRect.x0) - yesWidth) * 0.5f,
            y0,
            layout.popupRect.x0 + ((layout.popupRect.x1 - layout.popupRect.x0) - yesWidth) * 0.5f + yesWidth,
            y1,
        };
        return p;
    }

    const float noWidth = measureMaxLinePx(p.noLabel, p.buttonScalePx) + p.buttonPadX * 2.0f;
    p.noButton = {
        layout.popupRect.x1 - c.popupButtonSideInsetPx * layout.menuScale - noWidth,
        y0,
        layout.popupRect.x1 - c.popupButtonSideInsetPx * layout.menuScale,
        y1,
    };
    p.yesButton = {
        p.noButton.x0 - c.popupButtonGapPx * layout.menuScale - yesWidth,
        y0,
        p.noButton.x0 - c.popupButtonGapPx * layout.menuScale,
        y1,
    };
    return p;
}

} // namespace ui::pause_menu_layout

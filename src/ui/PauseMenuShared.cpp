#include "ui/PauseMenuShared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui::pause_menu_shared {
    Rect PauseMenuLayout::lineRect(const std::size_t visibleIndex) const {
        const float lineY = linesStartY + static_cast<float>(visibleIndex) * rowStep;
        const bool showScrollbar = totalLines > visibleLines;
        const float scrollbarReserve = showScrollbar ? 22.0f * menuScale : 0.0f;
        return Rect{
            .x0 = sectionX0 + 10.0f * menuScale,
            .y0 = lineY - 2.0f * menuScale,
            .x1 = sectionX1 - 10.0f * menuScale - scrollbarReserve,
            .y1 = lineY + (static_cast<float>(kFontH) + 3.5f) * rowScalePx + 2.0f * menuScale,
        };
    }

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
        maxChars = std::max(maxChars, lineChars);
        return static_cast<float>(maxChars) * scalePx;
    }

    float fitScaleForWidth(const std::string& s, const float preferredScalePx, const float maxWidthPx) {
        const float unitWidth = measureMaxLinePx(s, 1.0f);
        if (unitWidth <= 0.0f || maxWidthPx <= 0.0f) {
            return preferredScalePx;
        }
        return std::min(preferredScalePx, maxWidthPx / unitWidth);
    }

    std::string formatSpeedMultiplier(const double value) {
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

    std::string formatGravityMultiplier(const double value) {
        char buffer[32];
        const double multiplier = value / kDefaultGravityStrength;
        if (multiplier >= 10.0) {
            std::snprintf(buffer, sizeof(buffer), "%.0fX", multiplier);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.2fX", multiplier);
        }
        return buffer;
    }

    std::string formatRestitutionPercent(const double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0, 1.0) * 100.0);
        return buffer;
    }

    PauseMenuLayout buildPauseMenuLayout(
        const float width,
        const float height,
        const float uiScale,
        const OverlayRenderer::PauseMenuHud& hud)
    {
        PauseMenuLayout layout{};
        layout.width = width;
        layout.height = height;
        layout.menuScale = std::clamp(uiScale, 0.75f, 1.50f);
        layout.baseScalePx = kBaseScalePx * layout.menuScale;
        layout.tabsScalePx = layout.baseScalePx;
        layout.actionScalePx = layout.baseScalePx * 0.98f;
        layout.cardX0 = (width - std::min(width * 0.82f, 1240.0f)) * 0.5f;
        layout.cardY0 = (height - std::min(height * 0.86f, 920.0f)) * 0.5f;
        layout.cardX1 = layout.cardX0 + std::min(width * 0.82f, 1240.0f);
        layout.cardY1 = layout.cardY0 + std::min(height * 0.86f, 920.0f);

        const float statusSlotY = layout.cardY0 + 96.0f * layout.menuScale;
        const float statusReservedScalePx = layout.baseScalePx * 0.95f;
        const float statusReservedHeight = (static_cast<float>(kFontH) + 2.0f) * statusReservedScalePx;
        const float tabsSlotY = statusSlotY + statusReservedHeight + 16.0f * layout.menuScale;
        const float tabPadX = 12.0f * layout.menuScale;
        const float tabPadY = 7.0f * layout.menuScale;
        const float tabGap = 12.0f * layout.menuScale;
        const float tabHeight = (static_cast<float>(kFontH) + 2.0f) * layout.tabsScalePx + tabPadY * 2.0f;
        float tabX = layout.cardX0 + 18.0f * layout.menuScale;
        for (std::size_t i = 0; i < kPageTabLabels.size(); ++i) {
            const float tabWidth = measureMaxLinePx(kPageTabLabels[i], layout.tabsScalePx) + tabPadX * 2.0f;
            layout.tabRects[i] = Rect{tabX, tabsSlotY, tabX + tabWidth, tabsSlotY + tabHeight};
            tabX += tabWidth + tabGap;
        }

        const float buttonPadX = 12.0f * layout.menuScale;
        const float exitWidth = measureMaxLinePx("EXIT TO HOME", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.exitButton = Rect{
            .x0 = layout.cardX1 - 18.0f * layout.menuScale - exitWidth,
            .y0 = tabsSlotY,
            .x1 = layout.cardX1 - 18.0f * layout.menuScale,
            .y1 = tabsSlotY + tabHeight,
        };

        const float closeWidth = measureMaxLinePx("X", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.closeButton = Rect{
            .x0 = layout.cardX1 - 18.0f * layout.menuScale - closeWidth,
            .y0 = layout.cardY0 + 18.0f * layout.menuScale,
            .x1 = layout.cardX1 - 18.0f * layout.menuScale,
            .y1 = layout.cardY0 + 18.0f * layout.menuScale + tabHeight,
        };

        const float resetWidth = measureMaxLinePx("R", layout.tabsScalePx) + buttonPadX * 2.0f;
        layout.resetIcon = Rect{
            .x0 = layout.cardX0 + 18.0f * layout.menuScale,
            .y0 = layout.cardY0 + 18.0f * layout.menuScale,
            .x1 = layout.cardX0 + 18.0f * layout.menuScale + resetWidth,
            .y1 = layout.cardY0 + 18.0f * layout.menuScale + tabHeight,
        };

        const float footerReservedH = (static_cast<float>(kFontH) + 3.0f) * (layout.baseScalePx * 0.92f) + 18.0f * layout.menuScale;
        layout.settingsY0 = std::max(layout.cardY0 + 170.0f * layout.menuScale, tabsSlotY + tabHeight + 16.0f * layout.menuScale);
        layout.sectionX0 = layout.cardX0 + 26.0f * layout.menuScale;
        layout.sectionX1 = layout.cardX1 - 26.0f * layout.menuScale;
        layout.settingsY1 = layout.cardY1 - 20.0f * layout.menuScale - footerReservedH;
        if (layout.settingsY1 - layout.settingsY0 < 220.0f) {
            layout.settingsY0 = layout.settingsY1 - 220.0f;
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
                    maxControlReserve = std::max(maxControlReserve, 128.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Choice:
                case OverlayRenderer::PauseMenuControlType::Numeric:
                    maxControlReserve = std::max(maxControlReserve, 248.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Rebind:
                    maxControlReserve = std::max(maxControlReserve, 240.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::Action:
                    maxControlReserve = std::max(maxControlReserve, 120.0f * layout.menuScale);
                    break;
                case OverlayRenderer::PauseMenuControlType::None:
                default:
                    break;
            }
        }

        layout.rowScalePx = kSettingsScalePx * layout.menuScale * 1.8f;
        const float rowAreaWidth = (layout.sectionX1 - layout.sectionX0) - 28.0f * layout.menuScale - maxControlReserve;
        if (maxRowWidth > 0.0f && rowAreaWidth > 0.0f) {
            layout.rowScalePx = std::min(layout.rowScalePx, rowAreaWidth / maxRowWidth);
        }

        layout.sectionHeaderScalePx = fitScaleForWidth("SETTINGS", layout.rowScalePx * 1.18f, (layout.sectionX1 - layout.sectionX0) - 28.0f);
        const float sectionHeaderY = layout.settingsY0 + 10.0f * layout.menuScale;
        layout.linesStartY =
            sectionHeaderY + (static_cast<float>(kFontH) + 3.0f) * layout.sectionHeaderScalePx + 9.0f * layout.menuScale;
        layout.rowStep = (static_cast<float>(kFontH) + 4.0f) * layout.rowScalePx;
        layout.actionButtonH = (static_cast<float>(kFontH) + 2.0f) * layout.actionScalePx + 8.0f * layout.menuScale * 2.0f;
        const float actionReservedH = layout.actionButtonH + 20.0f * layout.menuScale;
        layout.contentY1 = layout.settingsY1 - 10.0f * layout.menuScale - actionReservedH;
        const float maxLines = std::floor((layout.contentY1 - layout.linesStartY) / layout.rowStep);
        layout.totalLines = hud.lines.size();
        layout.visibleLines = std::min<std::size_t>(layout.totalLines, static_cast<std::size_t>(std::max(0.0f, maxLines)));
        layout.firstLine = 0;
        if (layout.visibleLines > 0 && layout.totalLines > layout.visibleLines) {
            const std::size_t maxFirst = layout.totalLines - layout.visibleLines;
            layout.firstLine = std::min<std::size_t>(static_cast<std::size_t>(std::max(0, hud.firstVisibleLineIndex)), maxFirst);
            if (hud.selectedSettingLineIndex >= 0) {
                const auto selected = static_cast<std::size_t>(
                    std::clamp(hud.selectedSettingLineIndex, 0, static_cast<int>(layout.totalLines - 1)));
                if (selected < layout.firstLine) {
                    layout.firstLine = selected;
                } else if (selected >= layout.firstLine + layout.visibleLines) {
                    layout.firstLine = selected - layout.visibleLines + 1;
                }
            }
            layout.firstLine = std::min(layout.firstLine, maxFirst);
        }

        layout.popupRect = Rect{
            .x0 = layout.cardX0 + ((layout.cardX1 - layout.cardX0) - 320.0f * layout.menuScale) * 0.5f,
            .y0 = layout.cardY0 + ((layout.cardY1 - layout.cardY0) - 120.0f * layout.menuScale) * 0.5f,
            .x1 = 0.0f,
            .y1 = 0.0f,
        };
        layout.popupRect.x1 = layout.popupRect.x0 + 320.0f * layout.menuScale;
        layout.popupRect.y1 = layout.popupRect.y0 + 120.0f * layout.menuScale;

        if (hud.showApplyAction) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("APPLY CHANGES", layout.actionScalePx) + actionPadX * 2.0f;
            layout.applyAction = Rect{
                .x0 = layout.sectionX1 - 12.0f * layout.menuScale - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = layout.sectionX1 - 12.0f * layout.menuScale,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        if (hud.showResetWorldAction) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("RESET WORLD", layout.actionScalePx) + actionPadX * 2.0f;
            float actionX1 = layout.sectionX1 - 12.0f * layout.menuScale;
            if (hud.showApplyAction) {
                const float applyW = measureMaxLinePx("APPLY CHANGES", layout.actionScalePx) + actionPadX * 2.0f;
                actionX1 -= applyW + 12.0f * layout.menuScale;
            }
            layout.resetWorldAction = Rect{
                .x0 = actionX1 - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = actionX1,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        if (hud.showResetControlsAction) {
            const float actionPadX = 16.0f * layout.menuScale;
            const float actionW = measureMaxLinePx("RESET CONTROLS", layout.actionScalePx) + actionPadX * 2.0f;
            layout.resetControlsAction = Rect{
                .x0 = layout.sectionX1 - 12.0f * layout.menuScale - actionW,
                .y0 = layout.settingsY1 - 10.0f * layout.menuScale - layout.actionButtonH,
                .x1 = layout.sectionX1 - 12.0f * layout.menuScale,
                .y1 = layout.settingsY1 - 10.0f * layout.menuScale,
            };
        }

        return layout;
    }
} // namespace ui::pause_menu_shared

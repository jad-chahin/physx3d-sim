#include "ui/OverlayRendererInternal.h"

namespace overlay_renderer {
using namespace draw_shared;

namespace {

void pushTrianglePx(
    std::vector<float>& v,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float x2,
    const float y2)
{
    v.insert(v.end(), {x0, y0, x1, y1, x2, y2});
}

void pushDiscPx(
    std::vector<float>& v,
    const float cx,
    const float cy,
    const float radius,
    const int segments = 12)
{
    if (radius <= 0.0f || segments < 3) {
        return;
    }
    for (int i = 0; i < segments; ++i) {
        constexpr float kTau = 6.28318530717958647692f;
        const float a0 = kTau * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = kTau * static_cast<float>(i + 1) / static_cast<float>(segments);
        pushTrianglePx(
            v,
            cx,
            cy,
            cx + std::cos(a0) * radius,
            cy + std::sin(a0) * radius,
            cx + std::cos(a1) * radius,
            cy + std::sin(a1) * radius);
    }
}

const ButtonPalette& buttonPaletteForState(
    const ButtonStyle& style,
    const bool hovered,
    const bool selected)
{
    return selected ? style.selected : (hovered ? style.hover : style.normal);
}

void drawResetIconPx(
    std::vector<float>& out,
    const ui::pause_menu_layout::Rect& rect)
{
    const float width = rect.x1 - rect.x0;
    const float height = rect.y1 - rect.y0;
    const float cx = 0.5f * (rect.x0 + rect.x1);
    const float cy = 0.5f * (rect.y0 + rect.y1);
    const float radius = std::min(width, height) * 0.24f;
    const float thickness = std::max(1.4f, std::min(width, height) * 0.085f);
    constexpr int segments = 12;
    constexpr float kPi = 3.14159265359f;
    constexpr float startAngle = 0.55f * kPi;
    constexpr float endAngle = 2.08f * kPi;
    float prevX = cx + std::cos(startAngle) * radius;
    float prevY = cy + std::sin(startAngle) * radius;
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = startAngle + (endAngle - startAngle) * t;
        const float x = cx + std::cos(angle) * radius;
        const float y = cy + std::sin(angle) * radius;
        pushThickLinePx(out, prevX, prevY, x, y, thickness);
        prevX = x;
        prevY = y;
    }

    constexpr float headAngle = endAngle;
    const float tipX = cx + std::cos(headAngle) * radius;
    const float tipY = cy + std::sin(headAngle) * radius;
    const float tangentX = -std::sin(headAngle);
    const float tangentY = std::cos(headAngle);
    const float normalX = std::cos(headAngle);
    const float normalY = std::sin(headAngle);
    const float headLength = radius * 0.72f;
    const float headWidth = radius * 0.58f;
    pushTrianglePx(
        out,
        tipX,
        tipY,
        tipX - tangentX * headLength + normalX * headWidth * 0.5f,
        tipY - tangentY * headLength + normalY * headWidth * 0.5f,
        tipX - tangentX * headLength - normalX * headWidth * 0.5f,
        tipY - tangentY * headLength - normalY * headWidth * 0.5f);
}

void drawActionButtons(
    const ui::MenuView& menu,
    const ui::pause_menu_layout::PauseMenuLayout& layout,
    const ButtonStyle& style,
    const ui::ActionPlacement placement,
    const float padX,
    const float padY,
    const float textScalePx)
{
    for (std::size_t i = 0; i < menu.actions.size(); ++i) {
        const auto& action = menu.actions[i];
        if (action.placement != placement) {
            continue;
        }
        const auto& rect = layout.actionRect(i);
        drawButtonPx(style, rect, padX, padY, textScalePx, action.label, action.hovered, action.selected);
        if (action.id == static_cast<int>(ui::ActionKind::ResetSettings)) {
            drawResetIconPx(buttonPaletteForState(style, action.hovered, action.selected).text, rect);
        }
    }
}

} // namespace

void drawMenu(
    const ui::MenuView& menu,
    const Geometry& geometry,
    const Buffers& buffers)
{
    const float w = geometry.width;
    const float h = geometry.height;
    const ui::pause_menu_layout::PauseMenuLayout l =
        ui::pause_menu_layout::buildPauseMenuLayout(w, h, geometry.uiScale, menu);
    const float menuScale = l.menuScale;
    const float baseScalePx = l.baseScalePx;
    const float preferredTitleScalePx = kTitleScalePx * menuScale;

    pushQuadPx(buffers.screenDim, 0.0f, 0.0f, w, h);

    const float cardX0 = l.cardX0;
    const float cardY0 = l.cardY0;
    const float cardX1 = l.cardX1;
    const float cardY1 = l.cardY1;
    const float cardW = cardX1 - cardX0;
    pushQuadPx(buffers.panelFill, cardX0, cardY0, cardX1, cardY1);
    pushFramePx(buffers.panelFrame, cardX0, cardY0, cardX1, cardY1, 2.0f);

    const float centerX = w * 0.5f;
    const float infoWidth = cardW - 72.0f;
    const std::string title = menu.title.empty() ? "MENU" : menu.title;
    const std::string subtitle = menu.subtitle;
    const float titleScalePx = fitScaleForWidth(title, preferredTitleScalePx, infoWidth);
    const float subtitleScalePx =
        subtitle.empty() ? 0.0f : fitScaleForWidth(subtitle, baseScalePx * 1.05f, infoWidth);
    appendTextPx(
        buffers.textAccent,
        centerX - measureMaxLinePx(title, titleScalePx) * 0.5f,
        cardY0 + 18.0f * menuScale,
        titleScalePx,
        title);
    if (!subtitle.empty()) {
        appendTextPx(
            buffers.textMuted,
            centerX - measureMaxLinePx(subtitle, subtitleScalePx) * 0.5f,
            cardY0 + 64.0f * menuScale,
            subtitleScalePx,
            subtitle);
    }

    if (!menu.statusLine.empty()) {
        const float statusScalePx =
            fitScaleForWidth(menu.statusLine, baseScalePx * 0.95f, infoWidth);
        appendTextPx(
            menu.statusWarning ? buffers.textWarning : buffers.textAccent,
            centerX - measureMaxLinePx(menu.statusLine, statusScalePx) * 0.5f,
            cardY0 + 96.0f * menuScale,
            statusScalePx,
            menu.statusLine);
    }

    const float tabsScalePx = l.tabsScalePx;
    const float tabPadX = 12.0f * menuScale;
    const float tabPadY = 7.0f * menuScale;
    const ButtonStyle pauseButtonStyle{
        {buffers.panelFill, buffers.panelFrame, buffers.textMuted},
        {buffers.panelFrame, buffers.panelFrame, buffers.textPrimary},
        {buffers.panelFill, buffers.textPrimary, buffers.textPrimary},
    };
    const int maxTabIndex = static_cast<int>(menu.tabs.size()) - 1;
    const int activeTab = maxTabIndex >= 0 ? std::clamp(menu.activeTabIndex, 0, maxTabIndex) : -1;

    for (std::size_t i = 0; i < menu.tabs.size() && i < l.tabRects.size(); ++i) {
        drawButtonPx(
            pauseButtonStyle,
            l.tabRects[i],
            tabPadX,
            tabPadY,
            tabsScalePx,
            menu.tabs[i],
            menu.hoveredTabIndex == static_cast<int>(i),
            static_cast<int>(i) == activeTab);
    }

    drawActionButtons(
        menu,
        l,
        pauseButtonStyle,
        ui::ActionPlacement::Top,
        12.0f * menuScale,
        tabPadY,
        tabsScalePx);

    pushQuadPx(buffers.panelFill, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1);
    pushFramePx(buffers.panelFrame, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1, 1.5f);
    const std::string sectionTitle = menu.sectionTitle.empty() ? "SETTINGS" : menu.sectionTitle;
    appendTextPx(
        buffers.textAccent,
        l.sectionX0 + 14.0f * menuScale,
        l.settingsY0 + 10.0f * menuScale,
        l.sectionHeaderScalePx,
        sectionTitle);

    if (l.visibleLines > 0 && l.totalLines > l.visibleLines) {
        const float trackW = 8.0f * menuScale;
        const float trackX1 = l.sectionX1 - 8.0f * menuScale;
        const float trackX0 = trackX1 - trackW;
        const float trackY0 = l.linesStartY;
        const float trackY1 = l.contentY1;
        pushQuadPx(buffers.panelFill, trackX0, trackY0, trackX1, trackY1);
        pushFramePx(buffers.panelFrame, trackX0, trackY0, trackX1, trackY1, 1.0f);
        const float visibleRatio =
            std::clamp(static_cast<float>(l.visibleLines) / static_cast<float>(l.totalLines), 0.0f, 1.0f);
        const float thumbH = std::max(18.0f * menuScale, (trackY1 - trackY0) * visibleRatio);
        const auto maxFirst = static_cast<float>(l.totalLines - l.visibleLines);
        const float scrollT = maxFirst > 0.0f ? static_cast<float>(l.firstLine) / maxFirst : 0.0f;
        const float thumbTravel = std::max(0.0f, (trackY1 - trackY0) - thumbH);
        const float thumbY0 = trackY0 + thumbTravel * scrollT;
        pushQuadPx(
            buffers.accentFill,
            trackX0 + 1.0f,
            thumbY0 + 1.0f,
            trackX1 - 1.0f,
            thumbY0 + thumbH - 1.0f);
    }

    drawActionButtons(
        menu,
        l,
        pauseButtonStyle,
        ui::ActionPlacement::Bottom,
        16.0f * menuScale,
        8.0f * menuScale,
        l.actionScalePx);

    for (std::size_t i = 0; i < l.visibleLines; ++i) {
        const std::size_t lineIdx = l.firstLine + i;
        const auto& line = menu.rows[lineIdx];
        const bool selected = line.selected;
        const bool hovered = static_cast<int>(lineIdx) == menu.hoveredRowIndex;
        const bool disabled = line.disabled;
        const auto widgets = l.lineWidgets(i, line);
        const float lineY = l.linesStartY + static_cast<float>(i) * l.rowStep;

        if (disabled && line.kind != ui::RowKind::Header) {
            pushQuadPx(
                buffers.disabledOverlay,
                widgets.line.x0,
                widgets.line.y0,
                widgets.line.x1,
                widgets.line.y1);
        } else if (selected && line.kind != ui::RowKind::Header) {
            pushFramePx(
                buffers.textPrimary,
                widgets.line.x0,
                widgets.line.y0,
                widgets.line.x1,
                widgets.line.y1,
                1.5f);
        } else if (hovered && line.kind != ui::RowKind::Header) {
            pushQuadPx(buffers.panelFrame, widgets.line.x0, widgets.line.y0, widgets.line.x1, widgets.line.y1);
        }

        const float lineScalePx = line.kind == ui::RowKind::Header
            ? fitScaleForWidth(line.label, l.rowScalePx * 1.03f, (l.sectionX1 - l.sectionX0))
            : l.rowScalePx;
        appendTextPx(
            line.kind == ui::RowKind::Header ? buffers.textAccent
                        : (disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted)),
            l.sectionX0 + 14.0f * menuScale,
            lineY,
            lineScalePx,
            line.label);
        if (line.kind == ui::RowKind::Header) {
            continue;
        }

        const float valueScalePx = l.rowScalePx * 0.88f;
        const float buttonY0 = lineY - 2.0f * menuScale;
        const float buttonY1 =
            lineY + (static_cast<float>(kFontH) + 3.5f) * l.rowScalePx + 2.0f * menuScale;
        const auto drawInlineButton =
            [&](const float x0, const float x1, const std::string& text, const bool active) {
                if (active && !disabled && !selected) {
                    pushQuadPx(buffers.panelFill, x0, buttonY0, x1, buttonY1);
                }
                if (active) {
                    pushFramePx(
                        disabled ? buffers.panelFrame : buffers.textPrimary,
                        x0,
                        buttonY0,
                        x1,
                        buttonY1,
                        1.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted
                             : (active ? buffers.textPrimary : buffers.textMuted),
                    x0 + (x1 - x0 - measureMaxLinePx(text, valueScalePx)) * 0.5f,
                    lineY + 0.3f * menuScale,
                    valueScalePx,
                    text);
            };

        switch (line.kind) {
            case ui::RowKind::Toggle: {
                const std::string toggleText = line.boolValue ? "ON" : "OFF";
                const float textX0 = widgets.checkbox.x1 + 5.0f * menuScale;
                const float textW = measureMaxLinePx(toggleText, valueScalePx);
                const float textX =
                    textX0 + std::max(0.0f, (widgets.value.x1 - 12.0f * menuScale - textX0 - textW) * 0.5f);
                pushFramePx(
                    buffers.textMuted,
                    widgets.checkbox.x0,
                    widgets.checkbox.y0,
                    widgets.checkbox.x1,
                    widgets.checkbox.y1,
                    1.1f);
                if (line.boolValue) {
                    pushQuadPx(
                        disabled ? buffers.panelFrame : buffers.accentFill,
                        widgets.checkbox.x0 + 2.0f,
                        widgets.checkbox.y0 + 2.0f,
                        widgets.checkbox.x1 - 2.0f,
                        widgets.checkbox.y1 - 2.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted),
                    textX,
                    lineY + 3.3f * menuScale,
                    valueScalePx,
                    toggleText);
                break;
            }
            case ui::RowKind::Choice:
                drawInlineButton(widgets.leftArrow.x0, widgets.leftArrow.x1, "<", false);
                drawInlineButton(widgets.rightArrow.x0, widgets.rightArrow.x1, ">", false);
                drawInlineButton(widgets.value.x0, widgets.value.x1, line.value, false);
                break;
            case ui::RowKind::Rebind:
            case ui::RowKind::Header:
                if (!line.value.empty()) {
                    appendTextPx(
                        buffers.textMuted,
                        widgets.value.x0 +
                            ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.value, valueScalePx)) *
                                0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.value);
                }
                break;
        }
    }

    if (!menu.footerHint.empty()) {
        const float footerX0 = l.sectionX0;
        const float footerY0 = l.settingsY1 + 10.0f * menuScale;
        const float footerY1 = cardY1 - 16.0f * menuScale;
        pushQuadPx(buffers.panelFill, footerX0, footerY0, l.sectionX1, footerY1);
        pushFramePx(buffers.panelFrame, footerX0, footerY0, l.sectionX1, footerY1, 1.2f);
        appendTextPx(
            buffers.textMuted,
            footerX0 + 10.0f * menuScale,
            footerY0 + 6.0f * menuScale,
            fitScaleForWidth(
                menu.footerHint,
                baseScalePx * 0.92f,
                (l.sectionX1 - footerX0) - 18.0f * menuScale),
            menu.footerHint);
    }

    if (!menu.popup.visible) {
        return;
    }

    const auto popupButtons = ui::pause_menu_layout::buildPopupButtonsLayout(l, menu.popup);
    const std::string titleText =
        menu.popup.title.empty() ? "ARE YOU SURE?" : menu.popup.title;
    const std::string bodyText =
        menu.popup.body.empty() ? "CONFIRM ACTION" : menu.popup.body;

    pushQuadPx(buffers.popupBg, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1);
    pushFramePx(buffers.popupFrame, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1, 1.5f);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 14.0f * menuScale,
        baseScalePx * 0.98f,
        titleText);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 36.0f * menuScale,
        popupButtons.buttonScalePx,
        bodyText);

    if (menu.popup.hoverConfirm && !menu.popup.confirmSelected) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.yesButton.x0 + 1.0f,
            popupButtons.yesButton.y0 + 1.0f,
            popupButtons.yesButton.x1 - 1.0f,
            popupButtons.yesButton.y1 - 1.0f);
    }
    pushFramePx(
        menu.popup.confirmSelected ? buffers.popupText : buffers.popupFrame,
        popupButtons.yesButton.x0,
        popupButtons.yesButton.y0,
        popupButtons.yesButton.x1,
        popupButtons.yesButton.y1,
        menu.popup.confirmSelected ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.yesButton.x0 + popupButtons.buttonPadX,
        popupButtons.yesButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.yesLabel);

    const bool cancelSelected = !menu.popup.confirmSelected;
    if (menu.popup.hoverCancel && !cancelSelected) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.noButton.x0 + 1.0f,
            popupButtons.noButton.y0 + 1.0f,
            popupButtons.noButton.x1 - 1.0f,
            popupButtons.noButton.y1 - 1.0f);
    }
    pushFramePx(
        cancelSelected ? buffers.popupText : buffers.popupFrame,
        popupButtons.noButton.x0,
        popupButtons.noButton.y0,
        popupButtons.noButton.x1,
        popupButtons.noButton.y1,
        cancelSelected ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.noButton.x0 + popupButtons.buttonPadX,
        popupButtons.noButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.noLabel);
}

void drawHud(
    const Geometry& geometry,
    const double simSpeed,
    const double simElapsed,
    const double fps,
    const OverlayRenderer::SpatialHud& spatialHud,
    const OverlayRenderer::FocusMarker& focusMarker,
    const OverlayRenderer::FocusHint& focusHint,
    const bool showSimulationSpeed,
    const bool showFps,
    const bool showElapsedTime,
    const bool showMinimap,
    const bool showCoordinates,
    const Buffers& buffers)
{
    const float baseScalePx = kBaseScalePx * geometry.uiScale;
    const float hudRowStep = 20.0f * geometry.uiScale;
    constexpr float hudX0 = 16.0f;
    constexpr float hudY0 = 16.0f;
    constexpr float hudTextX = hudX0 + 10.0f;
    constexpr float hudTextY0 = hudY0 + 10.0f;
    char speedLine[64];
    char fpsLine[64];
    char timeLine[64];
    formatHudSpeed(speedLine, sizeof(speedLine), simSpeed);
    std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);
    formatElapsedTime(timeLine, sizeof(timeLine), simElapsed);

    float infoMaxLineW = 0.0f;
    int infoLineCount = 0;
    if (showSimulationSpeed) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(speedLine, baseScalePx));
        ++infoLineCount;
    }
    if (showFps) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(fpsLine, baseScalePx));
        ++infoLineCount;
    }
    if (showElapsedTime) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(timeLine, baseScalePx));
        ++infoLineCount;
    }

    const bool showInfoPanel = infoLineCount > 0;
    float infoPanelBottom = hudY0;
    if (showInfoPanel) {
        const float maxExpectedTimeWidth = measureMaxLinePx("TIME: 999D 23:59:59", baseScalePx);
        const float panelTextWidth = std::max(infoMaxLineW, maxExpectedTimeWidth);
        const float hudX1 = std::min(geometry.width - 16.0f, hudTextX + panelTextWidth + 20.0f);
        const float hudY1 = hudY0 + 16.0f + static_cast<float>(infoLineCount) * hudRowStep;
        pushQuadPx(buffers.panelFill, hudX0, hudY0, hudX1, hudY1);
        pushFramePx(buffers.panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);

        float lineY = hudTextY0;
        if (showSimulationSpeed) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, speedLine);
            lineY += hudRowStep;
        }
        if (showFps) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, fpsLine);
            lineY += hudRowStep;
        }
        if (showElapsedTime) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, timeLine);
        }
        infoPanelBottom = hudY1;
    }

    const float coordsScalePx = baseScalePx * 0.9f;
    if (showCoordinates) {
        char coordXLine[64];
        char coordYLine[64];
        char coordZLine[64];
        std::snprintf(coordXLine, sizeof(coordXLine), "X: %.2f", spatialHud.worldX);
        std::snprintf(coordYLine, sizeof(coordYLine), "Y: %.2f", spatialHud.worldY);
        std::snprintf(coordZLine, sizeof(coordZLine), "Z: %.2f", spatialHud.worldZ);

        constexpr float coordsX0 = hudX0;
        const float coordsY0 = showInfoPanel ? (infoPanelBottom + 14.0f * geometry.uiScale) : hudY0;
        constexpr float coordsTextX = coordsX0 + 10.0f;
        const float coordsTextY = coordsY0 + 9.0f;
        const float coordsWidth = std::max(
            {measureMaxLinePx(coordXLine, coordsScalePx),
             measureMaxLinePx(coordYLine, coordsScalePx),
             measureMaxLinePx(coordZLine, coordsScalePx)}) + 20.0f;
        const float coordsX1 = coordsX0 + coordsWidth + 10.0f;
        const float coordsY1 = coordsY0 + 12.0f + 3.0f * 18.0f * geometry.uiScale;
        pushQuadPx(buffers.panelFill, coordsX0, coordsY0, coordsX1, coordsY1);
        pushFramePx(buffers.panelFrame, coordsX0, coordsY0, coordsX1, coordsY1, 1.2f);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY, coordsScalePx, coordXLine);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY + 18.0f * geometry.uiScale, coordsScalePx, coordYLine);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY + 36.0f * geometry.uiScale, coordsScalePx, coordZLine);
    }

    if (showMinimap) {
        const float mapSize = 148.0f * geometry.uiScale;
        const float mapMargin = 16.0f * geometry.uiScale;
        const float mapX1 = geometry.width - mapMargin;
        const float mapY1 = geometry.height - mapMargin;
        const float mapX0 = mapX1 - mapSize;
        const float mapY0 = mapY1 - mapSize;
        const float mapCenterX = (mapX0 + mapX1) * 0.5f;
        const float mapCenterY = (mapY0 + mapY1) * 0.5f;
        const float mapInnerPad = 10.0f * geometry.uiScale;
        const float mapInnerX0 = mapX0 + mapInnerPad;
        const float mapInnerY0 = mapY0 + mapInnerPad;
        const float mapInnerX1 = mapX1 - mapInnerPad;
        const float mapInnerY1 = mapY1 - mapInnerPad;
        const float mapHalfSpanX = (mapInnerX1 - mapInnerX0) * 0.5f;
        const float mapHalfSpanY = (mapInnerY1 - mapInnerY0) * 0.5f;
        pushQuadPx(buffers.panelFill, mapX0, mapY0, mapX1, mapY1);
        pushFramePx(buffers.panelFrame, mapX0, mapY0, mapX1, mapY1, 1.5f);
        const float forwardLen = 10.0f * geometry.uiScale;
        const float baseBack = 5.0f * geometry.uiScale;
        const float halfWidth = 4.0f * geometry.uiScale;
        const float dirX = spatialHud.forwardX;
        const float dirY = spatialHud.forwardY;
        const float sideX = -dirY;
        const float sideY = dirX;
        const float tipX = mapCenterX + dirX * forwardLen;
        const float tipY = mapCenterY + dirY * forwardLen;
        const float baseX = mapCenterX - dirX * baseBack;
        const float baseY = mapCenterY - dirY * baseBack;
        const float leftX = baseX + sideX * halfWidth;
        const float leftY = baseY + sideY * halfWidth;
        const float rightX = baseX - sideX * halfWidth;
        const float rightY = baseY - sideY * halfWidth;
        pushTrianglePx(buffers.statusText, tipX, tipY, leftX, leftY, rightX, rightY);
        pushThickLinePx(
            buffers.panelFrame,
            leftX,
            leftY,
            tipX,
            tipY,
            std::max(1.0f, geometry.uiScale));
        pushThickLinePx(
            buffers.panelFrame,
            rightX,
            rightY,
            tipX,
            tipY,
            std::max(1.0f, geometry.uiScale));
        pushThickLinePx(
            buffers.panelFrame,
            leftX,
            leftY,
            rightX,
            rightY,
            std::max(1.0f, geometry.uiScale));
        for (const auto& marker : spatialHud.markers) {
            const float projectedX = mapCenterX + marker.xNorm * mapHalfSpanX;
            const float projectedY = mapCenterY + marker.yNorm * mapHalfSpanY;
            const float markerClampPad = 6.0f * geometry.uiScale;
            const ui::minimap::MarkerLayout markerLayout = ui::minimap::computeMarkerLayout(
                projectedX,
                projectedY,
                mapInnerX0,
                mapInnerY0,
                mapInnerX1,
                mapInnerY1,
                markerClampPad,
                marker.heightNorm,
                marker.edgeClamped);
            std::vector<float>& markerBuffer =
                marker.highlighted ? buffers.statusText : (marker.dynamicBody ? buffers.textPrimary : buffers.textMuted);
            float markerX = markerLayout.markerX;
            float markerY = markerLayout.markerY;
            float markerRadius = (marker.dynamicBody ? 2.7f : 2.1f) * geometry.uiScale;
            if (marker.highlighted) {
                const float outerRadius = 5.0f * geometry.uiScale;
                const float innerRadius = 2.5f * geometry.uiScale;
                markerRadius = outerRadius;
                if (markerLayout.showHeightCue) {
                    markerY = ui::minimap::clampMarkerYForCue(
                        markerY,
                        mapInnerY0,
                        mapInnerY1,
                        markerRadius,
                        2.0f * geometry.uiScale,
                        3.6f * geometry.uiScale,
                        marker.aboveCamera);
                }
                pushDiscPx(buffers.statusText, markerX, markerY, outerRadius);
                pushDiscPx(buffers.panelFill, markerX, markerY, innerRadius);
            } else {
                if (markerLayout.visualEdgeClamped) {
                    markerRadius = (marker.dynamicBody ? 2.9f : 2.3f) * geometry.uiScale;
                }
                if (markerLayout.showHeightCue) {
                    markerY = ui::minimap::clampMarkerYForCue(
                        markerY,
                        mapInnerY0,
                        mapInnerY1,
                        markerRadius,
                        2.0f * geometry.uiScale,
                        3.6f * geometry.uiScale,
                        marker.aboveCamera);
                }
                pushDiscPx(markerBuffer, markerX, markerY, markerRadius);
            }

            if (markerLayout.showHeightCue) {
                const float symbolGap = 2.0f * geometry.uiScale;
                const float symbolHalfWidth = 2.2f * geometry.uiScale;
                const float symbolHeight = 3.6f * geometry.uiScale;
                const float symbolCenterY = std::clamp(
                    markerY + (marker.aboveCamera ? -(markerRadius + symbolGap + symbolHeight * 0.5f)
                                                  : (markerRadius + symbolGap + symbolHeight * 0.5f)),
                    mapInnerY0 + symbolHeight * 0.5f,
                    mapInnerY1 - symbolHeight * 0.5f);
                const float symbolTipY = marker.aboveCamera
                    ? symbolCenterY - symbolHeight * 0.5f
                    : symbolCenterY + symbolHeight * 0.5f;
                const float symbolBaseY = marker.aboveCamera
                    ? symbolCenterY + symbolHeight * 0.5f
                    : symbolCenterY - symbolHeight * 0.5f;
                pushTrianglePx(
                    markerBuffer,
                    markerX,
                    symbolTipY,
                    markerX - symbolHalfWidth,
                    symbolBaseY,
                    markerX + symbolHalfWidth,
                    symbolBaseY);
            }
        }
    }

    if (focusMarker.visible) {
        drawFocusMarker(focusMarker, geometry, buffers);
    }

    drawFocusHint(focusHint, geometry, buffers, baseScalePx);
}

void drawFocusMarker(
    const OverlayRenderer::FocusMarker& focusMarker,
    const Geometry& geometry,
    const Buffers& buffers)
{
    if (!focusMarker.visible) {
        return;
    }

    const float radius = std::max(10.0f, focusMarker.radiusPx);
    const float gap = 7.0f * geometry.uiScale;
    const float armLength = std::clamp(radius * 0.45f, 10.0f * geometry.uiScale, 18.0f * geometry.uiScale);
    const float thickness = std::max(1.2f, 1.6f * geometry.uiScale);
    const float outlineThickness = thickness + 2.2f * geometry.uiScale;
    const float xInner0 = focusMarker.xPx - (radius + gap);
    const float xInner1 = focusMarker.xPx + (radius + gap);
    const float yInner0 = focusMarker.yPx - (radius + gap);
    const float yInner1 = focusMarker.yPx + (radius + gap);
    const float xOuter0 = xInner0 - armLength;
    const float xOuter1 = xInner1 + armLength;
    const float yOuter0 = yInner0 - armLength;
    const float yOuter1 = yInner1 + armLength;

    const auto drawBracket = [&](std::vector<float>& out, const float lineThickness) {
        pushThickLinePx(out, xOuter0, yInner0, xInner0, yInner0, lineThickness);
        pushThickLinePx(out, xInner0, yOuter0, xInner0, yInner0, lineThickness);

        pushThickLinePx(out, xInner1, yInner0, xOuter1, yInner0, lineThickness);
        pushThickLinePx(out, xInner1, yOuter0, xInner1, yInner0, lineThickness);

        pushThickLinePx(out, xOuter0, yInner1, xInner0, yInner1, lineThickness);
        pushThickLinePx(out, xInner0, yInner1, xInner0, yOuter1, lineThickness);

        pushThickLinePx(out, xInner1, yInner1, xOuter1, yInner1, lineThickness);
        pushThickLinePx(out, xInner1, yInner1, xInner1, yOuter1, lineThickness);
    };

    drawBracket(buffers.panelFill, outlineThickness);
    drawBracket(buffers.textPrimary, thickness);
}

void drawFocusHint(
    const OverlayRenderer::FocusHint& focusHint,
    const Geometry& geometry,
    const Buffers& buffers,
    const float baseScalePx)
{
    if (!focusHint.visible || focusHint.label.empty()) {
        return;
    }

    const float hintLabelScalePx = baseScalePx * 0.95f;
    const float hintActionScalePx = baseScalePx * 0.82f;
    const bool hasActionHint = !focusHint.actionHint.empty();
    const float labelWidth = measureMaxLinePx(focusHint.label, hintLabelScalePx);
    const float actionWidth = hasActionHint ? measureMaxLinePx(focusHint.actionHint, hintActionScalePx) : 0.0f;
    const float padX = 14.0f * geometry.uiScale;
    const float padY = 10.0f * geometry.uiScale;
    const float lineGap = 16.0f * geometry.uiScale;
    const float chipWidth = std::max(labelWidth, actionWidth) + padX * 2.0f;
    float chipHeight = padY * 2.0f + hintLabelScalePx * static_cast<float>(kFontH);
    if (hasActionHint) {
        chipHeight += lineGap + hintActionScalePx * static_cast<float>(kFontH);
    }

    const float chipX0 = (geometry.width - chipWidth) * 0.5f;
    const float chipY1 = geometry.height - 20.0f * geometry.uiScale;
    const float chipY0 = chipY1 - chipHeight;
    pushQuadPx(buffers.panelFill, chipX0, chipY0, chipX0 + chipWidth, chipY1);
    pushFramePx(buffers.panelFrame, chipX0, chipY0, chipX0 + chipWidth, chipY1, 1.4f);

    const float labelX = chipX0 + (chipWidth - labelWidth) * 0.5f;
    const float labelY = chipY0 + padY;
    appendTextPx(buffers.textPrimary, labelX, labelY, hintLabelScalePx, focusHint.label);

    if (hasActionHint) {
        const float actionX = chipX0 + (chipWidth - actionWidth) * 0.5f;
        const float actionY = labelY + hintLabelScalePx * static_cast<float>(kFontH) + lineGap;
        appendTextPx(buffers.textPrimary, actionX, actionY, hintActionScalePx, focusHint.actionHint);
    }
}

void drawFrozenIndicator(const Geometry& geometry, const Buffers& buffers) {
    constexpr float indicatorInset = 16.0f;
    const float baseScalePx = kBaseScalePx * geometry.uiScale * 0.92f;
    const char* label = "FROZEN";
    const float labelWidth = measureLinePx(label, baseScalePx);
    const float iconH = 26.0f * geometry.uiScale;
    const float iconW = 20.0f * geometry.uiScale;
    const float labelGap = 10.0f * geometry.uiScale;
    const float x1 = geometry.width - indicatorInset * geometry.uiScale;
    const float x0 = x1 - iconW;
    const float y0 = indicatorInset * geometry.uiScale;
    const float y1 = y0 + iconH;
    const float barW = std::max(4.0f, iconW * 0.24f);
    const float gap = std::max(4.5f, iconW * 0.20f);
    const float totalBarsW = barW * 2.0f + gap;
    const float leftX0 = x0 + (iconW - totalBarsW) * 0.5f;
    const float leftX1 = leftX0 + barW;
    const float rightX0 = leftX1 + gap;
    const float rightX1 = rightX0 + barW;
    const float barY0 = y0 + iconH * 0.08f;
    const float barY1 = y1 - iconH * 0.08f;
    const float labelX = x0 - labelGap - labelWidth;
    const float labelY = y0 + std::max(0.0f, (iconH - static_cast<float>(kFontH) * baseScalePx) * 0.5f);
    appendTextPx(buffers.frozenIndicator, labelX, labelY, baseScalePx, label);
    pushQuadPx(buffers.frozenIndicator, leftX0, barY0, leftX1, barY1);
    pushQuadPx(buffers.frozenIndicator, rightX0, barY0, rightX1, barY1);
}

void drawCrosshair(const Geometry& geometry, const Buffers& buffers) {
    const float cx = geometry.width * 0.5f;
    const float cy = geometry.height * 0.5f;
    pushQuadPx(buffers.crosshair, cx - 13.0f, cy - 1.0f, cx - 5.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx + 5.0f, cy - 1.0f, cx + 13.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 13.0f, cx + 1.0f, cy - 5.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy + 5.0f, cx + 1.0f, cy + 13.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
}

void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers)
{
    if (!targetHud.visible || targetHud.lines.empty()) {
        return;
    }

    const float baseScalePx = kBaseScalePx * geometry.uiScale * 0.88f;
    float maxLineW = 0.0f;
    std::string popup;
    popup.reserve(192);
    for (std::size_t i = 0; i < targetHud.lines.size(); ++i) {
        if (i > 0) {
            popup += '\n';
        }
        popup += targetHud.lines[i];
        maxLineW = std::max(maxLineW, measureLinePx(targetHud.lines[i], baseScalePx));
    }

    const float popupW = maxLineW + 24.0f;
    const float popupH =
        static_cast<float>((kFontH + 2) * static_cast<int>(targetHud.lines.size())) * baseScalePx +
        12.0f;
    const float px = targetHud.xPx - popupW * 0.5f;
    const float py = targetHud.yPx - popupH - 12.0f;
    pushQuadPx(buffers.popupBg, px, py, px + popupW, py + popupH);
    pushFramePx(buffers.popupFrame, px, py, px + popupW, py + popupH, 1.5f);
    appendTextPx(buffers.popupText, px + 8.0f, py + 6.0f, baseScalePx, popup);
}

} // namespace overlay_renderer

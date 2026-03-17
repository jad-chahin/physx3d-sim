#include "ui/OverlayRendererDrawShared.h"

namespace overlay_renderer {
using namespace draw_shared;

void drawHud(
    const Geometry& geometry,
    const bool simFrozen,
    const double simSpeed,
    const double fps,
    const std::vector<std::string>& hudDebugLines,
    const Buffers& buffers)
{
    const float baseScalePx = kBaseScalePx * geometry.uiScale;
    constexpr float hudX0 = 16.0f;
    constexpr float hudY0 = 16.0f;
    constexpr float hudTextX = hudX0 + 10.0f;
    constexpr float hudTextY0 = hudY0 + 10.0f;
    const float hudRowStep = 20.0f * geometry.uiScale;
    char speedLine[64];
    formatHudSpeed(speedLine, sizeof(speedLine), simSpeed);
    char fpsLine[64];
    std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);

    float maxLineW = 0.0f;
    maxLineW = std::max(maxLineW, measureMaxLinePx(simFrozen ? "WORLD: FROZEN" : "WORLD: RUNNING", baseScalePx));
    maxLineW = std::max(maxLineW, measureMaxLinePx("ESC: MENU", baseScalePx));
    maxLineW = std::max(maxLineW, measureMaxLinePx(speedLine, baseScalePx));
    maxLineW = std::max(maxLineW, measureMaxLinePx(fpsLine, baseScalePx));
    for (const auto& line : hudDebugLines) {
        maxLineW = std::max(maxLineW, measureMaxLinePx(line, baseScalePx));
    }

    const float desiredHudW = maxLineW + 20.0f;
    const float hudX1 = std::min(geometry.width - 16.0f, hudTextX + desiredHudW);
    const float hudY1 = hudY0 + 16.0f + (4.0f + static_cast<float>(hudDebugLines.size())) * hudRowStep;
    pushQuadPx(buffers.panelFill, hudX0, hudY0, hudX1, hudY1);
    pushFramePx(buffers.panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);
    pushQuadPx(buffers.accentFill, hudX0, hudY0, hudX1, hudY0 + 3.0f);

    appendTextPx(buffers.statusText, hudTextX, hudTextY0, baseScalePx, simFrozen ? "WORLD: FROZEN" : "WORLD: RUNNING");
    appendTextPx(buffers.textMuted, hudTextX, hudTextY0 + hudRowStep, baseScalePx, "ESC: MENU");
    appendTextPx(buffers.textPrimary, hudTextX, hudTextY0 + hudRowStep * 2.0f, baseScalePx, speedLine);
    appendTextPx(buffers.textPrimary, hudTextX, hudTextY0 + hudRowStep * 3.0f, baseScalePx, fpsLine);
    for (std::size_t i = 0; i < hudDebugLines.size(); ++i) {
        appendTextPx(
            buffers.textMuted,
            hudTextX,
            hudTextY0 + hudRowStep * (4.0f + static_cast<float>(i)),
            baseScalePx,
            hudDebugLines[i]);
    }
}

void drawCrosshair(const Geometry& geometry, const Buffers& buffers) {
    const float cx = geometry.width * 0.5f;
    const float cy = geometry.height * 0.5f;
    constexpr float gap = 5.0f;
    constexpr float len = 8.0f;
    constexpr float thick = 1.0f;
    pushQuadPx(buffers.crosshair, cx - len - gap, cy - thick, cx - gap, cy + thick);
    pushQuadPx(buffers.crosshair, cx + gap, cy - thick, cx + len + gap, cy + thick);
    pushQuadPx(buffers.crosshair, cx - thick, cy - len - gap, cx + thick, cy - gap);
    pushQuadPx(buffers.crosshair, cx - thick, cy + gap, cx + thick, cy + len + gap);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
}

void drawPathLines(
    const Geometry& geometry,
    const std::vector<OverlayRenderer::ScreenLine>& pathLines,
    const Buffers& buffers)
{
    (void)geometry;
    for (const auto& line : pathLines) {
        pushLinePx(buffers.pathLines, line.x0, line.y0, line.x1, line.y1);
    }
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
        static_cast<float>((kFontH + 2) * static_cast<int>(targetHud.lines.size())) * baseScalePx + 12.0f;
    const float px = targetHud.xPx - popupW * 0.5f;
    const float py = targetHud.yPx - popupH - 12.0f;

    pushQuadPx(buffers.popupBg, px, py, px + popupW, py + popupH);
    pushFramePx(buffers.popupFrame, px, py, px + popupW, py + popupH, 1.5f);
    pushQuadPx(buffers.accentFill, px, py, px + popupW, py + 3.0f);
    appendTextPx(buffers.popupText, px + 8.0f, py + 6.0f, baseScalePx, popup);
}

} // namespace overlay_renderer

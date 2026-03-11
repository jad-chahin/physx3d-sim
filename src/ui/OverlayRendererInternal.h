#ifndef PHYSICS3D_OVERLAYRENDERERINTERNAL_H
#define PHYSICS3D_OVERLAYRENDERERINTERNAL_H

#include "ui/OverlayRenderer.h"

#include <string>
#include <vector>

namespace overlay_renderer {

struct Geometry {
    float width;
    float height;
    float uiScale;
};

struct Buffers {
    std::vector<float>& screenDim;
    std::vector<float>& panelFill;
    std::vector<float>& panelFrame;
    std::vector<float>& accentFill;
    std::vector<float>& textPrimary;
    std::vector<float>& textMuted;
    std::vector<float>& textAccent;
    std::vector<float>& textWarning;
    std::vector<float>& statusText;
    std::vector<float>& crosshair;
    std::vector<float>& popupBg;
    std::vector<float>& popupFrame;
    std::vector<float>& popupText;
};

void drawPauseMenu(
    const OverlayRenderer::PauseMenuHud& pauseMenu,
    const Geometry& geometry,
    const Buffers& buffers);

void drawHud(
    const Geometry& geometry,
    bool simFrozen,
    double simSpeed,
    double fps,
    const std::vector<std::string>& hudDebugLines,
    const Buffers& buffers);

void drawCrosshair(const Geometry& geometry, const Buffers& buffers);

void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers);

} // namespace overlay_renderer

#endif // PHYSICS3D_OVERLAYRENDERERINTERNAL_H

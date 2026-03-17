#ifndef PHYSICS3D_UI_OVERLAYRENDERER_H
#define PHYSICS3D_UI_OVERLAYRENDERER_H

#include "ui/PauseMenu.h"

#include <glad/glad.h>
#include <array>
#include <string>
#include <vector>

class OverlayRenderer {
public:
    enum class PauseMenuControlType { None = 0, Toggle, Choice, Numeric, Rebind, Action };
    enum class PauseMenuAction { Apply = 0, ResetWorld, ResetControls, ResetIcon, Close, Exit, Count };

    struct PauseMenuActionState { bool visible = false, hovered = false, selected = false; };
    struct PauseMenuHudLine { std::string text, valueText, hintText; bool header = false, disabled = false; PauseMenuControlType controlType = PauseMenuControlType::None; bool boolValue = false, showArrows = false, showSlider = false; float sliderT = 0.0f; };
    struct TargetHud { bool visible = false; float xPx = 0.0f, yPx = 0.0f; std::vector<std::string> lines; };
    struct ScreenLine { float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f; };
    struct PauseMenuHud {
        bool visible = false, awaitingBind = false, selectedRowIsControl = false;
        std::array<PauseMenuActionState, static_cast<std::size_t>(PauseMenuAction::Count)> actions{};
        int hoveredPageTabIndex = -1, activePageIndex = 0, firstVisibleLineIndex = 0, selectedSettingLineIndex = -1, hoveredSettingLineIndex = -1, appliedSettingLineIndex = -1;
        bool showResetConfirm = false, hoverResetConfirmYes = false, hoverResetConfirmNo = false, selectedResetConfirmYes = false, selectedResetConfirmNo = false, resetConfirmSingleAcknowledge = false;
        std::string resetConfirmTitleText, resetConfirmBodyText, resetConfirmYesLabel, resetConfirmNoLabel, pendingAction, statusLine, footerHint;
        std::vector<PauseMenuHudLine> lines;
    };

    void init();
    void shutdown();
    void draw(int fbw, int fbh, bool simFrozen, double simSpeed, double fps, const PauseMenuHud& pauseMenu, const TargetHud& targetHud, const std::vector<ScreenLine>& pathLines, float uiScale, bool showHud, bool showCrosshair, const std::vector<std::string>& hudDebugLines) const;

private:
    GLuint program_ = 0, vao_ = 0, vbo_ = 0; GLint uViewport_ = -1, uColor_ = -1;
};

namespace ui { OverlayRenderer::PauseMenuHud toOverlayPauseMenuHud(const MenuView& view); }

namespace overlay_renderer {

struct Geometry { float width, height, uiScale; };
struct Buffers {
    std::vector<float>& screenDim; std::vector<float>& panelFill; std::vector<float>& panelFrame; std::vector<float>& accentFill; std::vector<float>& textPrimary;
    std::vector<float>& textMuted; std::vector<float>& textAccent; std::vector<float>& textWarning; std::vector<float>& statusText; std::vector<float>& crosshair;
    std::vector<float>& pathLines; std::vector<float>& popupBg; std::vector<float>& popupFrame; std::vector<float>& popupAccent; std::vector<float>& popupText;
};

void drawPauseMenu(const OverlayRenderer::PauseMenuHud& pauseMenu, const Geometry& geometry, const Buffers& buffers);
void drawHud(const Geometry& geometry, bool simFrozen, double simSpeed, double fps, const std::vector<std::string>& hudDebugLines, const Buffers& buffers);
void drawCrosshair(const Geometry& geometry, bool simFrozen, const Buffers& buffers);
void drawPathLines(const Geometry& geometry, const std::vector<OverlayRenderer::ScreenLine>& pathLines, const Buffers& buffers);
void drawTargetPopup(const OverlayRenderer::TargetHud& targetHud, const Geometry& geometry, const Buffers& buffers);

} // namespace overlay_renderer

#endif // PHYSICS3D_UI_OVERLAYRENDERER_H


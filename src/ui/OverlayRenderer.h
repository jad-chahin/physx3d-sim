#ifndef PHYSICS3D_OVERLAYRENDERER_H
#define PHYSICS3D_OVERLAYRENDERER_H

#include <glad/glad.h>
#include <string>
#include <vector>


class OverlayRenderer {
public:
    enum class PauseMenuControlType {
        None = 0,
        Toggle,
        Choice,
        Numeric,
        Rebind,
        Action,
    };

    struct PauseMenuHudLine {
        std::string text;
        std::string valueText;
        std::string hintText;
        bool header = false;
        bool disabled = false;
        PauseMenuControlType controlType = PauseMenuControlType::None;
        bool boolValue = false;
        bool showArrows = false;
        bool showSlider = false;
        float sliderT = 0.0f;
    };

    struct TargetHud {
        bool visible = false;
        float xPx = 0.0f;
        float yPx = 0.0f;
        std::vector<std::string> lines;
    };

    struct ScreenLine {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    struct PauseMenuHud {
        bool visible = false;
        bool awaitingBind = false;
        bool selectedRowIsControl = false;
        bool showApplyAction = false;
        bool hoverApplyAction = false;
        bool selectedApplyAction = false;
        bool showResetWorldAction = false;
        bool hoverResetWorldAction = false;
        bool selectedResetWorldAction = false;
        bool showResetControlsAction = false;
        bool hoverResetControlsAction = false;
        bool selectedResetControlsAction = false;
        bool showResetIcon = false;
        bool hoverResetIcon = false;
        bool selectedResetIcon = false;
        bool selectedCloseAction = false;
        bool selectedExitAction = false;
        bool showResetConfirm = false;
        bool hoverResetConfirmYes = false;
        bool hoverResetConfirmNo = false;
        bool selectedResetConfirmYes = false;
        bool selectedResetConfirmNo = false;
        std::string resetConfirmBodyText;
        std::string pendingAction;
        std::string statusLine;
        int activePageIndex = 0;
        int firstVisibleLineIndex = 0;
        int selectedSettingLineIndex = -1;
        int hoveredSettingLineIndex = -1;
        int appliedSettingLineIndex = -1;
        std::string footerHint;
        std::vector<PauseMenuHudLine> lines;
    };

    void init();
    void shutdown();

    // Draw freeze indicator, optional pause menu, and target HUD.
    void draw(
        int fbw,
        int fbh,
        bool simFrozen,
        double simSpeed,
        double fps,
        const PauseMenuHud& pauseMenu,
        const TargetHud& targetHud,
        const std::vector<ScreenLine>& pathLines,
        float uiScale,
        bool showHud,
        bool showCrosshair,
        const std::vector<std::string>& hudDebugLines) const;

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    GLint uViewport_ = -1;
    GLint uColor_ = -1;
};

#endif //PHYSICS3D_OVERLAYRENDERER_H

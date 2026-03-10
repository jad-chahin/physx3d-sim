//
// Created by jchah on 2026-01-04.
//

#ifndef PHYSICS3D_DEBUGOVERLAY_H
#define PHYSICS3D_DEBUGOVERLAY_H

#include <glad/glad.h>
#include <string>
#include <vector>


class DebugOverlay {
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

    struct PauseMenuHud {
        bool visible = false;
        bool awaitingBind = false;
        bool selectedRowIsControl = false;
        bool showDisplayApplyAction = false;
        bool hoverDisplayApplyAction = false;
        bool showResetControlsAction = false;
        bool hoverResetControlsAction = false;
        bool showResetIcon = false;
        bool hoverResetIcon = false;
        bool showResetConfirm = false;
        bool hoverResetConfirmYes = false;
        bool hoverResetConfirmNo = false;
        std::string pendingAction;
        std::string statusLine;
        int activePageIndex = 0;
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

#endif //PHYSICS3D_DEBUGOVERLAY_H

#ifndef PHYSICS3D_UI_OVERLAYRENDERER_H
#define PHYSICS3D_UI_OVERLAYRENDERER_H

#include "ui/PauseMenu.h"

#include <glad/glad.h>
#include <string>
#include <vector>

class OverlayRenderer {
public:
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

    void init();
    void shutdown();
    void draw(
        int fbw,
        int fbh,
        bool simFrozen,
        double simSpeed,
        double fps,
        const ui::MenuView& menu,
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

#endif // PHYSICS3D_UI_OVERLAYRENDERER_H

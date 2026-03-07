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
    struct TargetHud {
        bool visible = false;
        float xPx = 0.0f;
        float yPx = 0.0f;
        float restitution = 0.0f;
        float staticFriction = 0.0f;
        float dynamicFriction = 0.0f;
    };

    struct PauseMenuHud {
        bool visible = false;
        bool awaitingBind = false;
        std::string pendingAction;
        int selectedControlIndex = -1;
        std::vector<std::string> settingLines;
        std::vector<std::string> controlLines;
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
        const TargetHud& targetHud) const;

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    GLint uViewport_ = -1;
    GLint uColor_ = -1;
};

#endif //PHYSICS3D_DEBUGOVERLAY_H

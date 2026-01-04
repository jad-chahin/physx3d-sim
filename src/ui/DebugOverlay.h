//
// Created by jchah on 2026-01-04.
//

#ifndef PHYSICS3D_DEBUGOVERLAY_H
#define PHYSICS3D_DEBUGOVERLAY_H

#include <glad/glad.h>


class DebugOverlay {
public:
    void init();
    void shutdown();

    // Draw top-right controls and paused indicator
    void draw(int fbw, int fbh, bool paused) const;

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    GLint uViewport_ = -1;
    GLint uColor_ = -1;
};

#endif //PHYSICS3D_DEBUGOVERLAY_H
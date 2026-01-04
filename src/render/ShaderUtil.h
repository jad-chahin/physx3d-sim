//
// Created by jchah on 2026-01-04.
//

#ifndef PHYSICS3D_SHADERUTIL_H
#define PHYSICS3D_SHADERUTIL_H

#include <glad/glad.h>


void APIENTRY glDebugCallback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam);

GLuint compileShader(GLenum type, const char* src);
GLuint linkProgram(GLuint vs, GLuint fs);

extern const char* kVertSrc;
extern const char* kFragSrc;

#endif //PHYSICS3D_SHADERUTIL_H
//
// Created by jchah on 2026-01-04.
//

#include "ShaderUtil.h"
#include <iostream>


void APIENTRY glDebugCallback(
    const GLenum source, const GLenum type, const GLuint id, const GLenum severity,
    const GLsizei length, const GLchar* message, const void* userParam)
{
    (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
    std::cerr << "[GL DEBUG] " << message << "\n";
}

GLuint compileShader(const GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile failed:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint linkProgram(const GLuint vs, const GLuint fs) {
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::cerr << "Program link failed:\n" << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}


const char* kVertSrc = R"GLSL(#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Instance model matrix columns
layout (location = 2) in vec4 iM0;
layout (location = 3) in vec4 iM1;
layout (location = 4) in vec4 iM2;
layout (location = 5) in vec4 iM3;

uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    vec4 worldPos4 = model * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;

    mat3 normalMat = transpose(inverse(mat3(model)));
    vWorldNormal = normalize(normalMat * aNormal);

    gl_Position = uProj * uView * worldPos4;
}
)GLSL";

const char* kFragSrc = R"GLSL(#version 330 core

in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform vec3 uLightDir;   // world space (direction *towards* the light)
uniform vec3 uBaseColor;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vWorldNormal);
    vec3 l = normalize(uLightDir);

    float diff = max(0.0, dot(n, l));
    vec3 col = uBaseColor * (uAmbient + (1.0 - uAmbient) * diff);

    FragColor = vec4(col, 1.0);
}
)GLSL";

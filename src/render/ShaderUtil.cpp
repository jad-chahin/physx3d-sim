#include "ShaderUtil.h"
#include <iostream>


void APIENTRY glDebugCallback(
    const GLenum source, const GLenum type, const GLuint id, const GLenum severity,
    const GLsizei length, const GLchar* message, const void* userParam)
{
    (void)source; (void)type; (void)length; (void)userParam;

    // Filter chatty driver notifications (e.g., buffer placement info).
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    const char* severityText = "UNKNOWN";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: severityText = "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityText = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW: severityText = "LOW"; break;
        default: break;
    }

    std::cerr << "[GL DEBUG][" << severityText << "][id=" << id << "] " << message << "\n";
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

out vec3 vWorldNormal;
out vec3 vLocalNormal;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    vec4 worldPos4 = model * vec4(aPos, 1.0);
    vLocalNormal = normalize(aNormal);

    mat3 normalMat = transpose(inverse(mat3(model)));
    vWorldNormal = normalize(normalMat * aNormal);

    gl_Position = uProj * uView * worldPos4;
}
)GLSL";

const char* kFragSrc = R"GLSL(#version 330 core

in vec3 vWorldNormal;
in vec3 vLocalNormal;

uniform vec3 uLightDir;   // world space (direction *towards* the light)
uniform vec3 uBaseColor;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vWorldNormal);
    vec3 localN = normalize(vLocalNormal);
    vec3 l = normalize(uLightDir);

    float diff = max(0.0, dot(n, l));
    vec3 albedo = uBaseColor * vec3(0.96, 0.92, 0.74);

    float hemisphere = 0.5 + 0.5 * dot(localN, normalize(vec3(0.22, 0.96, 0.18)));
    albedo *= mix(0.78, 1.12, hemisphere);

    float majorMarker = smoothstep(0.22, 0.92, dot(localN, normalize(vec3(0.84, 0.18, -0.50))));
    albedo = mix(albedo, uBaseColor * vec3(1.18, 1.06, 0.72), majorMarker * 0.48);

    float minorMarker = smoothstep(0.38, 0.96, dot(localN, normalize(vec3(-0.36, -0.14, 0.92))));
    albedo = mix(albedo, albedo * vec3(0.78, 0.82, 0.92), minorMarker * 0.32);

    vec3 col = albedo * (uAmbient + (1.0 - uAmbient) * diff);

    FragColor = vec4(col, 1.0);
}
)GLSL";

#include "render/FrameRenderer.h"

#include "render/SceneLighting.h"
#include "render/ShaderUtil.h"
#include "render/SphereMesh.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace render_scene {
namespace {

bool sameVec2(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

} // namespace

constexpr auto kPathVertSrc = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aWorldPos;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aWorldPos, 1.0);
}
)GLSL";

constexpr auto kPathFragSrc = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";

constexpr auto kSkyVertSrc = R"GLSL(
#version 330 core
out vec2 vClipPos;
void main() {
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 pos = positions[gl_VertexID];
    vClipPos = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr auto kSkyFragSrc = R"GLSL(
#version 330 core
in vec2 vClipPos;
out vec4 FragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uGroundColor;
uniform vec3 uSunGlowColor;
uniform vec3 uSkyAccentColor;
uniform int uBackdropPreset;
uniform float uStarIntensity;
uniform vec3 uStateTintColor;
uniform float uStateTintStrength;

const float PI = 3.14159265359;

vec3 reconstructRayDir(vec2 clipPos) {
    vec4 worldFar = uInvViewProj * vec4(clipPos, 1.0, 1.0);
    vec3 worldPos = worldFar.xyz / worldFar.w;
    return normalize(worldPos - uCameraPos);
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p) {
    return vec2(
        hash21(p + vec2(0.0, 0.0)),
        hash21(p + vec2(19.1, 7.7)));
}

vec2 sphereUv(vec3 dir) {
    float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;
    return vec2(u, v);
}

float starLayer(vec2 uv, vec2 scale, float threshold, float radius, vec2 seed) {
    vec2 grid = uv * scale;
    vec2 cell = floor(grid);
    vec2 local = fract(grid) - 0.5;
    vec2 jitter = (hash22(cell + seed) - 0.5) * 0.78;
    float distanceToStar = length(local - jitter * 0.58);
    float rarity = step(threshold, hash21(cell + seed * 1.73));
    return smoothstep(radius, 0.0, distanceToStar) * rarity;
}

vec3 starField(vec3 rayDir) {
    vec2 uv = sphereUv(normalize(rayDir));
    vec2 starUv = vec2(uv.x, sin((uv.y - 0.5) * PI) * 0.5 + 0.5);
    vec3 stars = vec3(0.0);

    float starA = starLayer(starUv, vec2(210.0, 108.0), 0.9966, 0.080, vec2(3.1, 7.9));
    float tintMixA = hash21(floor(starUv * vec2(210.0, 108.0)) + vec2(5.0, 9.0));
    vec3 tintA = mix(vec3(0.72, 0.80, 1.0), vec3(1.0, 0.92, 0.80), tintMixA);
    stars += tintA * starA * 3.0;

    float starB = starLayer(starUv, vec2(360.0, 186.0), 0.9987, 0.066, vec2(11.0, 2.0));
    float tintMixB = hash21(floor(starUv * vec2(360.0, 186.0)) + vec2(8.0, 14.0));
    vec3 tintB = mix(vec3(0.80, 0.86, 1.0), vec3(1.0, 0.96, 0.88), tintMixB);
    stars += tintB * starB * 4.4;

    return stars * uStarIntensity;
}

vec3 atmosphericBackdrop(vec3 rayDir) {
    float zenithMix = smoothstep(0.06, 0.92, max(rayDir.y, 0.0));
    vec3 sky = mix(uSkyHorizonColor, uSkyZenithColor, zenithMix);

    float skyMix = smoothstep(-0.24, 0.12, rayDir.y);
    vec3 color = mix(uGroundColor, sky, skyMix);

    float horizonGlow = 1.0 - smoothstep(0.02, 0.38, abs(rayDir.y));
    color += uSkyHorizonColor * horizonGlow * 0.10;

    float sunAmount = max(dot(rayDir, normalize(uLightDir)), 0.0);
    float sunHalo = pow(sunAmount, 24.0);
    float sunDisc = pow(sunAmount, 640.0);
    color += uSunGlowColor * (0.30 * sunHalo + 1.15 * sunDisc);
    return color;
}

vec3 deepSpaceBackdrop(vec3 rayDir) {
    float zenithMix = smoothstep(0.0, 1.0, clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 sky = mix(uSkyHorizonColor, uSkyZenithColor, zenithMix);
    float hemisphereMix = smoothstep(-0.45, 0.35, rayDir.y);
    vec3 color = mix(uGroundColor, sky, hemisphereMix);
    float galacticBand = pow(max(1.0 - abs(rayDir.y + 0.08), 0.0), 5.0);
    color += mix(uSkyHorizonColor, uSkyAccentColor, 0.45) * galacticBand * 0.08;

    float sunAmount = max(dot(rayDir, normalize(uLightDir)), 0.0);
    color += uSunGlowColor * (0.10 * pow(sunAmount, 72.0) + 0.22 * pow(sunAmount, 1600.0));
    return color;
}

void main() {
    vec3 rayDir = reconstructRayDir(vClipPos);
    vec3 color = atmosphericBackdrop(rayDir);
    if (uBackdropPreset == 1) {
        color = deepSpaceBackdrop(rayDir) + starField(rayDir);
    }
    color = mix(color, uStateTintColor, uStateTintStrength);

    FragColor = vec4(color, 1.0);
}
)GLSL";

constexpr auto kShadowVertSrc = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec4 iM0;
layout (location = 3) in vec4 iM1;
layout (location = 4) in vec4 iM2;
layout (location = 5) in vec4 iM3;
layout (location = 8) in vec4 iEmissiveColorStrength;
layout (location = 9) in vec4 iSurfaceParams;

uniform mat4 uLightViewProj;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);
    gl_Position = uLightViewProj * model * vec4(aPos, 1.0);
}
)GLSL";

constexpr auto kShadowFragSrc = R"GLSL(
#version 330 core
void main() {
}
)GLSL";

constexpr auto kPostVertSrc = R"GLSL(
#version 330 core
out vec2 vUv;
void main() {
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 pos = positions[gl_VertexID];
    vUv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr auto kBloomExtractFragSrc = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform float uThreshold;

void main() {
    vec3 color = texture(uSceneColor, vUv).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float soft = clamp((luminance - uThreshold) / max(uThreshold * 0.6, 1e-4), 0.0, 1.0);
    vec3 bloom = max(color - vec3(uThreshold), vec3(0.0)) + color * soft * 0.18;
    FragColor = vec4(bloom, 1.0);
}
)GLSL";

constexpr auto kBloomBlurFragSrc = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uSourceTexture;
uniform vec2 uTexelSize;
uniform vec2 uDirection;

void main() {
    vec2 axis = uDirection * uTexelSize;
    vec3 color = texture(uSourceTexture, vUv).rgb * 0.227027;
    color += texture(uSourceTexture, vUv + axis * 1.384615).rgb * 0.316216;
    color += texture(uSourceTexture, vUv - axis * 1.384615).rgb * 0.316216;
    color += texture(uSourceTexture, vUv + axis * 3.230769).rgb * 0.070270;
    color += texture(uSourceTexture, vUv - axis * 3.230769).rgb * 0.070270;
    FragColor = vec4(color, 1.0);
}
)GLSL";

constexpr auto kPostFragSrc = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform sampler2D uBloomTexture;
uniform float uExposure;
uniform float uGamma;
uniform float uBloomStrength;
uniform float uContrast;
uniform float uSaturation;
uniform vec2 uCameraNearFar;
uniform vec3 uFogNearColor;
uniform vec3 uFogFarColor;
uniform vec2 uFogDistances;

vec3 toneMapAces(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float linearDepth(float depthSample) {
    float z = depthSample * 2.0 - 1.0;
    return (2.0 * uCameraNearFar.x * uCameraNearFar.y) /
        (uCameraNearFar.y + uCameraNearFar.x - z * (uCameraNearFar.y - uCameraNearFar.x));
}

vec3 applyGrade(vec3 color) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 graded = mix(vec3(luminance), color, uSaturation);
    graded = clamp((graded - 0.5) * uContrast + 0.5, 0.0, 1.0);
    graded *= vec3(1.008, 1.0, 0.994);
    return clamp(graded, 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(uSceneColor, vUv).rgb;
    float depth = texture(uSceneDepth, vUv).r;
    if (depth < 0.999999) {
        float viewDepth = linearDepth(depth);
        float fogFactor = clamp((viewDepth - uFogDistances.x) / max(uFogDistances.y - uFogDistances.x, 1e-4), 0.0, 1.0);
        fogFactor = fogFactor * fogFactor * (3.0 - 2.0 * fogFactor);
        vec3 fogColor = mix(uFogNearColor, uFogFarColor, fogFactor);
        hdrColor = mix(hdrColor, fogColor, fogFactor * 0.82);
    }
    if (uBloomStrength > 0.0) {
        hdrColor += texture(uBloomTexture, vUv).rgb * uBloomStrength;
    }
    vec3 exposed = hdrColor * uExposure;
    vec3 mapped = toneMapAces(exposed);
    vec3 displayColor = pow(applyGrade(mapped), vec3(1.0 / uGamma));
    FragColor = vec4(displayColor, 1.0);
}
)GLSL";

FrameRenderer::~FrameRenderer()
{
    shutdown();
}

bool FrameRenderer::init()
{
    if (initialized_) {
        return true;
    }
    if (!initSceneResources_()) {
        resources_.release();
        return false;
    }
    if (!initSkyResources_()) {
        sky_.release();
        resources_.release();
        return false;
    }
    if (!initShadowResources_()) {
        shadow_.release();
        sky_.release();
        resources_.release();
        return false;
    }
    if (!initPostProcessResources_()) {
        postProcess_.release();
        shadow_.release();
        sky_.release();
        resources_.release();
        return false;
    }
    if (!initBloomResources_()) {
        bloom_.release();
        postProcess_.release();
        shadow_.release();
        sky_.release();
        resources_.release();
        return false;
    }
    overlay_.init();
    scratch_ = {};
    appliedViewport_ = {-1, -1};
    initialized_ = true;
    return true;
}

bool FrameRenderer::initSceneResources_()
{
    std::vector<float> sphereVerts;
    std::vector<std::uint32_t> sphereIdx;
    buildSphereMesh(16, 32, sphereVerts, sphereIdx);
    resources_.sphereIndexCount = static_cast<GLsizei>(sphereIdx.size());

    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    if (vs == 0 || fs == 0) {
        std::cerr << "Shader compile failed; exiting.\n";
        return false;
    }

    resources_.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (resources_.program == 0) {
        std::cerr << "Program link failed; exiting.\n";
        return false;
    }

    resources_.uView = glGetUniformLocation(resources_.program, "uView");
    resources_.uProj = glGetUniformLocation(resources_.program, "uProj");
    resources_.uCameraPos = glGetUniformLocation(resources_.program, "uCameraPos");
    resources_.uLightDir = glGetUniformLocation(resources_.program, "uLightDir");
    resources_.uLightColor = glGetUniformLocation(resources_.program, "uLightColor");
    resources_.uSkyZenithColor = glGetUniformLocation(resources_.program, "uSkyZenithColor");
    resources_.uSkyHorizonColor = glGetUniformLocation(resources_.program, "uSkyHorizonColor");
    resources_.uGroundColor = glGetUniformLocation(resources_.program, "uGroundColor");
    resources_.uLightViewProj = glGetUniformLocation(resources_.program, "uLightViewProj");
    resources_.uShadowMap = glGetUniformLocation(resources_.program, "uShadowMap");
    resources_.uShadowTexelSize = glGetUniformLocation(resources_.program, "uShadowTexelSize");
    resources_.uLocalLightCount = glGetUniformLocation(resources_.program, "uLocalLightCount");
    resources_.uLocalLightPosRange = glGetUniformLocation(resources_.program, "uLocalLightPosRange[0]");
    resources_.uLocalLightColorIntensity = glGetUniformLocation(resources_.program, "uLocalLightColorIntensity[0]");
    if (resources_.uView < 0 || resources_.uProj < 0 || resources_.uCameraPos < 0 ||
        resources_.uLightDir < 0 || resources_.uLightColor < 0 ||
        resources_.uSkyZenithColor < 0 || resources_.uSkyHorizonColor < 0 ||
        resources_.uGroundColor < 0 ||
        resources_.uLightViewProj < 0 || resources_.uShadowMap < 0 ||
        resources_.uShadowTexelSize < 0 || resources_.uLocalLightCount < 0 ||
        resources_.uLocalLightPosRange < 0 || resources_.uLocalLightColorIntensity < 0)
    {
        std::cerr << "Missing uniform(s). Check shader names match exactly.\n";
    }

    glUseProgram(resources_.program);
    glUniform1i(resources_.uShadowMap, 0);
    glUseProgram(0);

    const GLuint pathVs = compileShader(GL_VERTEX_SHADER, kPathVertSrc);
    const GLuint pathFs = compileShader(GL_FRAGMENT_SHADER, kPathFragSrc);
    if (pathVs == 0 || pathFs == 0) {
        std::cerr << "Path shader compile failed; exiting.\n";
        return false;
    }

    resources_.pathProgram = linkProgram(pathVs, pathFs);
    glDeleteShader(pathVs);
    glDeleteShader(pathFs);
    if (resources_.pathProgram == 0) {
        std::cerr << "Path program link failed; exiting.\n";
        return false;
    }

    resources_.pathUViewProj = glGetUniformLocation(resources_.pathProgram, "uViewProj");
    resources_.pathUColor = glGetUniformLocation(resources_.pathProgram, "uColor");

    glGenVertexArrays(1, &resources_.sphereVao);
    glGenBuffers(1, &resources_.sphereVbo);
    glGenBuffers(1, &resources_.sphereEbo);
    glGenBuffers(1, &resources_.instanceVbo);
    glGenVertexArrays(1, &resources_.pathVao);
    glGenBuffers(1, &resources_.pathVbo);

    glBindVertexArray(resources_.sphereVao);

    glBindBuffer(GL_ARRAY_BUFFER, resources_.sphereVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereVerts.size() * sizeof(float)),
        sphereVerts.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, resources_.sphereEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sphereIdx.size() * sizeof(std::uint32_t)),
        sphereIdx.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, resources_.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    constexpr auto instanceStride = static_cast<GLsizei>(sizeof(SceneInstanceData));
    for (GLuint attrib = 0; attrib < 4; ++attrib) {
        glEnableVertexAttribArray(2 + attrib);
        glVertexAttribPointer(
            2 + attrib,
            4,
            GL_FLOAT,
            GL_FALSE,
            instanceStride,
            reinterpret_cast<void*>(offsetof(SceneInstanceData, model) + attrib * sizeof(glm::vec4)));
        glVertexAttribDivisor(2 + attrib, 1);
    }
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(
        6,
        4,
        GL_FLOAT,
        GL_FALSE,
        instanceStride,
        reinterpret_cast<void*>(offsetof(SceneInstanceData, albedoRoughness)));
    glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(
        7,
        4,
        GL_FLOAT,
        GL_FALSE,
        instanceStride,
        reinterpret_cast<void*>(offsetof(SceneInstanceData, metalnessParams)));
    glVertexAttribDivisor(7, 1);
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(
        8,
        4,
        GL_FLOAT,
        GL_FALSE,
        instanceStride,
        reinterpret_cast<void*>(offsetof(SceneInstanceData, emissiveColorStrength)));
    glVertexAttribDivisor(8, 1);
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(
        9,
        4,
        GL_FLOAT,
        GL_FALSE,
        instanceStride,
        reinterpret_cast<void*>(offsetof(SceneInstanceData, surfaceParams)));
    glVertexAttribDivisor(9, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(resources_.pathVao);
    glBindBuffer(GL_ARRAY_BUFFER, resources_.pathVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(glm::vec3)), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return true;
}

bool FrameRenderer::initSkyResources_()
{
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kSkyVertSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kSkyFragSrc);
    if (vs == 0 || fs == 0) {
        std::cerr << "Sky shader compile failed; exiting.\n";
        return false;
    }

    sky_.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (sky_.program == 0) {
        std::cerr << "Sky program link failed; exiting.\n";
        return false;
    }

    sky_.uInvViewProj = glGetUniformLocation(sky_.program, "uInvViewProj");
    sky_.uCameraPos = glGetUniformLocation(sky_.program, "uCameraPos");
    sky_.uLightDir = glGetUniformLocation(sky_.program, "uLightDir");
    sky_.uSkyZenithColor = glGetUniformLocation(sky_.program, "uSkyZenithColor");
    sky_.uSkyHorizonColor = glGetUniformLocation(sky_.program, "uSkyHorizonColor");
    sky_.uGroundColor = glGetUniformLocation(sky_.program, "uGroundColor");
    sky_.uSunGlowColor = glGetUniformLocation(sky_.program, "uSunGlowColor");
    sky_.uSkyAccentColor = glGetUniformLocation(sky_.program, "uSkyAccentColor");
    sky_.uBackdropPreset = glGetUniformLocation(sky_.program, "uBackdropPreset");
    sky_.uStarIntensity = glGetUniformLocation(sky_.program, "uStarIntensity");
    sky_.uStateTintColor = glGetUniformLocation(sky_.program, "uStateTintColor");
    sky_.uStateTintStrength = glGetUniformLocation(sky_.program, "uStateTintStrength");
    if (sky_.uInvViewProj < 0 || sky_.uCameraPos < 0 || sky_.uLightDir < 0 ||
        sky_.uSkyZenithColor < 0 || sky_.uSkyHorizonColor < 0 ||
        sky_.uGroundColor < 0 || sky_.uSunGlowColor < 0 ||
        sky_.uSkyAccentColor < 0 || sky_.uBackdropPreset < 0 ||
        sky_.uStarIntensity < 0 || sky_.uStateTintColor < 0 || sky_.uStateTintStrength < 0)
    {
        std::cerr << "Missing sky uniform(s). Check shader names match exactly.\n";
    }

    glGenVertexArrays(1, &sky_.fullscreenVao);
    return true;
}

bool FrameRenderer::initShadowResources_()
{
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kShadowVertSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kShadowFragSrc);
    if (vs == 0 || fs == 0) {
        std::cerr << "Shadow shader compile failed; exiting.\n";
        return false;
    }

    shadow_.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (shadow_.program == 0) {
        std::cerr << "Shadow program link failed; exiting.\n";
        return false;
    }

    shadow_.uLightViewProj = glGetUniformLocation(shadow_.program, "uLightViewProj");
    if (shadow_.uLightViewProj < 0) {
        std::cerr << "Missing shadow uniform(s). Check shader names match exactly.\n";
    }

    glGenTextures(1, &shadow_.depthTexture);
    glBindTexture(GL_TEXTURE_2D, shadow_.depthTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        shadow_.mapResolution,
        shadow_.mapResolution,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float borderColor[4]{1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &shadow_.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_.framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        shadow_.depthTexture,
        0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer is incomplete; exiting.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool FrameRenderer::initPostProcessResources_()
{
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kPostVertSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kPostFragSrc);
    if (vs == 0 || fs == 0) {
        std::cerr << "Post-process shader compile failed; exiting.\n";
        return false;
    }

    postProcess_.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (postProcess_.program == 0) {
        std::cerr << "Post-process program link failed; exiting.\n";
        return false;
    }

    postProcess_.uSceneColor = glGetUniformLocation(postProcess_.program, "uSceneColor");
    postProcess_.uSceneDepth = glGetUniformLocation(postProcess_.program, "uSceneDepth");
    postProcess_.uBloomTexture = glGetUniformLocation(postProcess_.program, "uBloomTexture");
    postProcess_.uExposure = glGetUniformLocation(postProcess_.program, "uExposure");
    postProcess_.uGamma = glGetUniformLocation(postProcess_.program, "uGamma");
    postProcess_.uBloomStrength = glGetUniformLocation(postProcess_.program, "uBloomStrength");
    postProcess_.uContrast = glGetUniformLocation(postProcess_.program, "uContrast");
    postProcess_.uSaturation = glGetUniformLocation(postProcess_.program, "uSaturation");
    postProcess_.uCameraNearFar = glGetUniformLocation(postProcess_.program, "uCameraNearFar");
    postProcess_.uFogNearColor = glGetUniformLocation(postProcess_.program, "uFogNearColor");
    postProcess_.uFogFarColor = glGetUniformLocation(postProcess_.program, "uFogFarColor");
    postProcess_.uFogDistances = glGetUniformLocation(postProcess_.program, "uFogDistances");

    glUseProgram(postProcess_.program);
    glUniform1i(postProcess_.uSceneColor, 0);
    glUniform1i(postProcess_.uSceneDepth, 1);
    glUniform1i(postProcess_.uBloomTexture, 2);
    glUseProgram(0);

    glGenFramebuffers(1, &postProcess_.hdrFramebuffer);
    glGenTextures(1, &postProcess_.hdrColorTexture);
    glGenTextures(1, &postProcess_.hdrDepthTexture);
    glGenVertexArrays(1, &postProcess_.fullscreenVao);
    return true;
}

bool FrameRenderer::initBloomResources_()
{
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kPostVertSrc);
    const GLuint extractFs = compileShader(GL_FRAGMENT_SHADER, kBloomExtractFragSrc);
    if (vs == 0 || extractFs == 0) {
        std::cerr << "Bloom extract shader compile failed; exiting.\n";
        return false;
    }

    bloom_.extractProgram = linkProgram(vs, extractFs);
    glDeleteShader(extractFs);
    if (bloom_.extractProgram == 0) {
        glDeleteShader(vs);
        std::cerr << "Bloom extract program link failed; exiting.\n";
        return false;
    }

    const GLuint blurFs = compileShader(GL_FRAGMENT_SHADER, kBloomBlurFragSrc);
    if (blurFs == 0) {
        glDeleteShader(vs);
        std::cerr << "Bloom blur shader compile failed; exiting.\n";
        return false;
    }

    bloom_.blurProgram = linkProgram(vs, blurFs);
    glDeleteShader(vs);
    glDeleteShader(blurFs);
    if (bloom_.blurProgram == 0) {
        std::cerr << "Bloom blur program link failed; exiting.\n";
        return false;
    }

    bloom_.extractUSceneColor = glGetUniformLocation(bloom_.extractProgram, "uSceneColor");
    bloom_.extractUThreshold = glGetUniformLocation(bloom_.extractProgram, "uThreshold");
    bloom_.blurUSourceTexture = glGetUniformLocation(bloom_.blurProgram, "uSourceTexture");
    bloom_.blurUTexelSize = glGetUniformLocation(bloom_.blurProgram, "uTexelSize");
    bloom_.blurUDirection = glGetUniformLocation(bloom_.blurProgram, "uDirection");

    glUseProgram(bloom_.extractProgram);
    glUniform1i(bloom_.extractUSceneColor, 0);
    glUseProgram(bloom_.blurProgram);
    glUniform1i(bloom_.blurUSourceTexture, 0);
    glUseProgram(0);

    glGenFramebuffers(2, bloom_.framebuffers);
    glGenTextures(2, bloom_.textures);
    return true;
}

bool FrameRenderer::resizeHdrTarget_(const FramebufferSize& framebufferSize)
{
    const int width = std::max(1, framebufferSize.w);
    const int height = std::max(1, framebufferSize.h);
    if (postProcess_.framebufferSize.w == width && postProcess_.framebufferSize.h == height) {
        return true;
    }

    glBindTexture(GL_TEXTURE_2D, postProcess_.hdrColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, postProcess_.hdrDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, postProcess_.hdrFramebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        postProcess_.hdrColorTexture,
        0);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        postProcess_.hdrDepthTexture,
        0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "HDR framebuffer is incomplete; exiting.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    postProcess_.framebufferSize = {width, height};
    return true;
}

bool FrameRenderer::resizeSkyCacheTarget_(const FramebufferSize& framebufferSize)
{
    const int width = std::max(1, framebufferSize.w);
    const int height = std::max(1, framebufferSize.h);
    if (skyCache_.framebufferSize.w == width && skyCache_.framebufferSize.h == height) {
        return true;
    }

    if (skyCache_.framebuffer == 0) {
        glGenFramebuffers(1, &skyCache_.framebuffer);
    }
    if (skyCache_.colorTexture == 0) {
        glGenTextures(1, &skyCache_.colorTexture);
    }

    glBindTexture(GL_TEXTURE_2D, skyCache_.colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, skyCache_.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, skyCache_.colorTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Sky cache framebuffer is incomplete; exiting.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    skyCache_.framebufferSize = {width, height};
    skyCacheValid_ = false;
    return true;
}

bool FrameRenderer::skyCacheMatches_(const SkyCacheKey& key) const
{
    return skyCacheValid_ &&
        skyCacheKey_.framebufferSize.w == key.framebufferSize.w &&
        skyCacheKey_.framebufferSize.h == key.framebufferSize.h &&
        sameVec3(skyCacheKey_.forward, key.forward) &&
        sameVec2(skyCacheKey_.projScale, key.projScale) &&
        sameVec3(skyCacheKey_.lightDir, key.lightDir) &&
        sameVec3(skyCacheKey_.skyZenithColor, key.skyZenithColor) &&
        sameVec3(skyCacheKey_.skyHorizonColor, key.skyHorizonColor) &&
        sameVec3(skyCacheKey_.groundColor, key.groundColor) &&
        sameVec3(skyCacheKey_.sunGlowColor, key.sunGlowColor) &&
        sameVec3(skyCacheKey_.skyAccentColor, key.skyAccentColor) &&
        skyCacheKey_.backdropPreset == key.backdropPreset &&
        skyCacheKey_.starIntensity == key.starIntensity &&
        sameVec3(skyCacheKey_.stateTintColor, key.stateTintColor) &&
        skyCacheKey_.stateTintStrength == key.stateTintStrength;
}

bool FrameRenderer::resizeBloomTargets_(const FramebufferSize& framebufferSize)
{
    const FramebufferSize bloomSize{
        std::max(1, framebufferSize.w / 2),
        std::max(1, framebufferSize.h / 2),
    };
    if (bloom_.framebufferSize.w == bloomSize.w && bloom_.framebufferSize.h == bloomSize.h) {
        return true;
    }

    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, bloom_.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomSize.w, bloomSize.h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, bloom_.framebuffers[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_.textures[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Bloom framebuffer is incomplete; exiting.\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    bloom_.framebufferSize = bloomSize;
    return true;
}

void FrameRenderer::shutdown()
{
    if (!initialized_) {
        return;
    }
    bloom_.release();
    overlay_.shutdown();
    postProcess_.release();
    shadow_.release();
    skyCache_.release();
    sky_.release();
    resources_.release();
    scratch_ = {};
    lightingCacheKey_ = {};
    skyCacheKey_ = {};
    cachedLighting_ = {};
    appliedViewport_ = {-1, -1};
    lightingCacheValid_ = false;
    skyCacheValid_ = false;
    initialized_ = false;
}

void FrameRenderer::render(const FrameInput& input)
{
    if (!initialized_ || input.sceneView == nullptr || input.sceneSnapshot == nullptr || input.overlay == nullptr) {
        return;
    }

    prepareSceneInstances_(*input.sceneSnapshot);
    const LightingCacheKey lightingKey{
        input.sceneSnapshot->instanceRevision,
        shadow_.mapResolution,
        input.sceneView->cameraPosition,
        input.sceneView->forward,
    };
    const bool lightingKeyChanged =
        !lightingCacheValid_ ||
        lightingCacheKey_.instanceRevision != lightingKey.instanceRevision ||
        lightingCacheKey_.shadowMapResolution != lightingKey.shadowMapResolution ||
        lightingCacheKey_.cameraPosition.x != lightingKey.cameraPosition.x ||
        lightingCacheKey_.cameraPosition.y != lightingKey.cameraPosition.y ||
        lightingCacheKey_.cameraPosition.z != lightingKey.cameraPosition.z ||
        lightingCacheKey_.forward.x != lightingKey.forward.x ||
        lightingCacheKey_.forward.y != lightingKey.forward.y ||
        lightingCacheKey_.forward.z != lightingKey.forward.z;
    if (lightingKeyChanged) {
        cachedLighting_ = buildSceneLighting(
            *input.sceneView,
            *input.sceneSnapshot,
            scratch_.instanceScratch,
            shadow_.mapResolution);
        lightingCacheKey_ = lightingKey;
        lightingCacheValid_ = true;
        if (!scratch_.instanceScratch.empty()) {
            renderShadowPass_(cachedLighting_);
        }
    }
    applyViewport_(input.framebufferSize);
    if (!resizeHdrTarget_(input.framebufferSize)) {
        return;
    }
    if (!resizeSkyCacheTarget_(input.framebufferSize)) {
        return;
    }
    const bool bloomEnabled = cachedLighting_.bloomStrength > 0.0f;
    if (bloomEnabled && !resizeBloomTargets_(input.framebufferSize)) {
        return;
    }
    const bool menuVisible = input.overlay->menu != nullptr && input.overlay->menu->visible;
    const glm::vec3 baseColor = sceneBaseColor_(input.simFrozen, menuVisible);
    const float stateTintStrength = input.simFrozen && !menuVisible ? 0.18f : 0.0f;
    updateSkyCache_(
        input.framebufferSize,
        *input.sceneView,
        cachedLighting_,
        baseColor,
        stateTintStrength);
    bindSceneTarget_(input.framebufferSize);
    clearSceneDepth_();
    copySkyCacheToSceneTarget_(input.framebufferSize);
    renderScenePass_(input, cachedLighting_);
    if (input.showWorldPaths) {
        renderWorldPathPass_(input);
    }
    if (bloomEnabled) {
        renderBloomPass_();
    }
    compositeSceneTarget_(input.framebufferSize, *input.sceneView, cachedLighting_);
    renderOverlayPass_(input);
}

void FrameRenderer::applyViewport_(const FramebufferSize& framebufferSize)
{
    if (appliedViewport_.w == framebufferSize.w && appliedViewport_.h == framebufferSize.h) {
        return;
    }
    glViewport(0, 0, framebufferSize.w, framebufferSize.h);
    appliedViewport_ = framebufferSize;
}

void FrameRenderer::prepareSceneInstances_(const SceneSnapshot& snapshot)
{
    if (resources_.instanceVbo == 0) {
        return;
    }
    if (scratch_.instanceBufferState.uploadedRevision == snapshot.instanceRevision) {
        return;
    }
    buildSceneInstances(scratch_.instanceScratch, snapshot);
    uploadSceneInstances(resources_.instanceVbo, scratch_.instanceBufferState, scratch_.instanceScratch);
    scratch_.instanceBufferState.uploadedRevision = snapshot.instanceRevision;
}

glm::vec3 FrameRenderer::sceneBaseColor_(const bool simFrozen, const bool menuVisible)
{
    if (simFrozen && !menuVisible) {
        return {0.035f, 0.055f, 0.075f};
    }
    return {0.05f, 0.05f, 0.08f};
}

void FrameRenderer::bindShadowTarget_() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_.framebuffer);
    glViewport(0, 0, shadow_.mapResolution, shadow_.mapResolution);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void FrameRenderer::renderShadowPass_(const SceneLighting& lighting) const
{
    bindShadowTarget_();
    glUseProgram(shadow_.program);
    glUniformMatrix4fv(shadow_.uLightViewProj, 1, GL_FALSE, &lighting.shadowViewProj[0][0]);
    glBindVertexArray(resources_.sphereVao);
    glDrawElementsInstanced(
        GL_TRIANGLES,
        resources_.sphereIndexCount,
        GL_UNSIGNED_INT,
        nullptr,
        static_cast<GLsizei>(scratch_.instanceScratch.size()));
    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
}

void FrameRenderer::bindSceneTarget_(const FramebufferSize& framebufferSize) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, postProcess_.hdrFramebuffer);
    glViewport(0, 0, framebufferSize.w, framebufferSize.h);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void FrameRenderer::clearSceneDepth_()
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

void FrameRenderer::updateSkyCache_(
    const FramebufferSize& framebufferSize,
    const SceneView& sceneView,
    const SceneLighting& lighting,
    const glm::vec3& stateTintColor,
    const float stateTintStrength)
{
    const SkyCacheKey key{
        framebufferSize,
        sceneView.forward,
        glm::vec2(sceneView.proj[0][0], sceneView.proj[1][1]),
        lighting.direction,
        lighting.skyZenithColor,
        lighting.skyHorizonColor,
        lighting.groundColor,
        lighting.sunGlowColor,
        lighting.skyAccentColor,
        lighting.backdropPreset,
        lighting.starIntensity,
        stateTintColor,
        stateTintStrength,
    };
    if (skyCacheMatches_(key)) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, skyCache_.framebuffer);
    glViewport(0, 0, framebufferSize.w, framebufferSize.h);
    renderSkyPass_(sceneView, lighting, stateTintColor, stateTintStrength);
    skyCacheKey_ = key;
    skyCacheValid_ = true;
}

void FrameRenderer::copySkyCacheToSceneTarget_(const FramebufferSize& framebufferSize) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, skyCache_.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcess_.hdrFramebuffer);
    glBlitFramebuffer(
        0,
        0,
        framebufferSize.w,
        framebufferSize.h,
        0,
        0,
        framebufferSize.w,
        framebufferSize.h,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, postProcess_.hdrFramebuffer);
}

void FrameRenderer::renderSkyPass_(
    const SceneView& sceneView,
    const SceneLighting& lighting,
    const glm::vec3& stateTintColor,
    const float stateTintStrength) const
{
    const glm::mat4 invViewProj = glm::inverse(sceneView.viewProj);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glUseProgram(sky_.program);
    glUniformMatrix4fv(sky_.uInvViewProj, 1, GL_FALSE, &invViewProj[0][0]);
    glUniform3fv(sky_.uCameraPos, 1, &sceneView.cameraPosition[0]);
    glUniform3fv(sky_.uLightDir, 1, &lighting.direction[0]);
    glUniform3fv(sky_.uSkyZenithColor, 1, &lighting.skyZenithColor[0]);
    glUniform3fv(sky_.uSkyHorizonColor, 1, &lighting.skyHorizonColor[0]);
    glUniform3fv(sky_.uGroundColor, 1, &lighting.groundColor[0]);
    glUniform3fv(sky_.uSunGlowColor, 1, &lighting.sunGlowColor[0]);
    glUniform3fv(sky_.uSkyAccentColor, 1, &lighting.skyAccentColor[0]);
    glUniform1i(sky_.uBackdropPreset, static_cast<int>(lighting.backdropPreset));
    glUniform1f(sky_.uStarIntensity, lighting.starIntensity);
    glUniform3fv(sky_.uStateTintColor, 1, &stateTintColor[0]);
    glUniform1f(sky_.uStateTintStrength, stateTintStrength);
    glBindVertexArray(sky_.fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void FrameRenderer::renderScenePass_(const FrameInput& input, const SceneLighting& lighting)
{
    if (scratch_.instanceScratch.empty()) {
        return;
    }
    drawBodies(
        ScenePassResources{
            resources_.program,
            resources_.uView,
            resources_.uProj,
            resources_.uCameraPos,
            resources_.uLightDir,
            resources_.uLightColor,
            resources_.uSkyZenithColor,
            resources_.uSkyHorizonColor,
            resources_.uGroundColor,
            resources_.uLightViewProj,
            resources_.uShadowMap,
            resources_.uShadowTexelSize,
            resources_.uLocalLightCount,
            resources_.uLocalLightPosRange,
            resources_.uLocalLightColorIntensity,
            resources_.sphereVao,
            shadow_.depthTexture,
            resources_.sphereIndexCount,
        },
        *input.sceneView,
        lighting,
        scratch_.instanceScratch);
}

void FrameRenderer::renderWorldPathPass_(const FrameInput& input)
{
    if (input.sceneSnapshot->pathTrails.empty()) {
        return;
    }
    drawPathTrails(
        resources_.pathVao,
        resources_.pathVbo,
        resources_.pathProgram,
        resources_.pathUViewProj,
        resources_.pathUColor,
        *input.sceneView,
        scratch_.pathBufferState,
        scratch_.pathPointsScratch,
        scratch_.pathDrawStarts,
        scratch_.pathDrawCounts,
        input.sceneSnapshot->pathTrails,
        input.sceneSnapshot->pathTrailsRevision,
        input.pathColorIndex,
        input.simFrozen);
}

void FrameRenderer::renderBloomPass_() const
{
    if (bloom_.framebufferSize.w <= 0 || bloom_.framebufferSize.h <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glViewport(0, 0, bloom_.framebufferSize.w, bloom_.framebufferSize.h);

    glBindFramebuffer(GL_FRAMEBUFFER, bloom_.framebuffers[0]);
    glUseProgram(bloom_.extractProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postProcess_.hdrColorTexture);
    glUniform1f(bloom_.extractUThreshold, 1.15f);
    glBindVertexArray(postProcess_.fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(bloom_.blurProgram);
    glUniform2f(
        bloom_.blurUTexelSize,
        1.0f / static_cast<float>(bloom_.framebufferSize.w),
        1.0f / static_cast<float>(bloom_.framebufferSize.h));

    glBindFramebuffer(GL_FRAMEBUFFER, bloom_.framebuffers[1]);
    glBindTexture(GL_TEXTURE_2D, bloom_.textures[0]);
    glUniform2f(bloom_.blurUDirection, 1.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, bloom_.framebuffers[0]);
    glBindTexture(GL_TEXTURE_2D, bloom_.textures[1]);
    glUniform2f(bloom_.blurUDirection, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameRenderer::compositeSceneTarget_(
    const FramebufferSize& framebufferSize,
    const SceneView& sceneView,
    const SceneLighting& lighting) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferSize.w, framebufferSize.h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(postProcess_.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postProcess_.hdrColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, postProcess_.hdrDepthTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(
        GL_TEXTURE_2D,
        lighting.bloomStrength > 0.0f ? bloom_.textures[0] : postProcess_.hdrColorTexture);
    glUniform1f(postProcess_.uExposure, 1.0f);
    glUniform1f(postProcess_.uGamma, 2.2f);
    glUniform1f(postProcess_.uBloomStrength, lighting.bloomStrength);
    glUniform1f(postProcess_.uContrast, 1.04f);
    glUniform1f(postProcess_.uSaturation, 1.05f);
    glUniform2f(postProcess_.uCameraNearFar, sceneView.nearPlane, sceneView.farPlane);
    glUniform3fv(postProcess_.uFogNearColor, 1, &lighting.fogNearColor[0]);
    glUniform3fv(postProcess_.uFogFarColor, 1, &lighting.fogFarColor[0]);
    glUniform2f(postProcess_.uFogDistances, lighting.fogNearDistance, lighting.fogFarDistance);

    glBindVertexArray(postProcess_.fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameRenderer::renderOverlayPass_(const FrameInput& input) const
{
    const OverlayPassInput& overlayInput = *input.overlay;
    if (overlayInput.menu == nullptr || overlayInput.spatialHud == nullptr || overlayInput.focusMarker == nullptr ||
        overlayInput.focusHint == nullptr ||
        overlayInput.targetHud == nullptr ||
        overlayInput.interfaceSettings == nullptr)
    {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    overlay_.draw(
        input.framebufferSize.w,
        input.framebufferSize.h,
        input.simFrozen,
        input.simSpeed,
        input.simElapsed,
        input.fps,
        *overlayInput.menu,
        *overlayInput.spatialHud,
        *overlayInput.focusMarker,
        *overlayInput.focusHint,
        *overlayInput.targetHud,
        overlayInput.uiScale,
        overlayInput.interfaceSettings->showSimulationSpeed,
        overlayInput.interfaceSettings->showFps,
        overlayInput.interfaceSettings->showElapsedTime,
        overlayInput.interfaceSettings->showMinimap,
        overlayInput.interfaceSettings->showCoordinates,
        overlayInput.interfaceSettings->showCrosshair);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace render_scene

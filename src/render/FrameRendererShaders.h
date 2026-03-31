#ifndef PHYSICS3D_RENDER_FRAMERENDERERSHADERS_H
#define PHYSICS3D_RENDER_FRAMERENDERERSHADERS_H

namespace render_scene {

inline constexpr auto kPathVertSrc = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aWorldPos;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aWorldPos, 1.0);
}
)GLSL";

inline constexpr auto kPathFragSrc = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";

inline constexpr auto kSkyVertSrc = R"GLSL(
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

inline constexpr auto kSkyFragSrc = R"GLSL(
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

inline constexpr auto kShadowVertSrc = R"GLSL(
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

inline constexpr auto kShadowFragSrc = R"GLSL(
#version 330 core
void main() {
}
)GLSL";

inline constexpr auto kPostVertSrc = R"GLSL(
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

inline constexpr auto kBloomExtractFragSrc = R"GLSL(
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

inline constexpr auto kBloomBlurFragSrc = R"GLSL(
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

inline constexpr auto kPostFragSrc = R"GLSL(
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

} // namespace render_scene

#endif // PHYSICS3D_RENDER_FRAMERENDERERSHADERS_H

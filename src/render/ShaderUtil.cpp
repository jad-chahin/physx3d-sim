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
layout (location = 6) in vec4 iAlbedoRoughness;
layout (location = 7) in vec4 iMetalnessParams;
layout (location = 8) in vec4 iEmissiveColorStrength;
layout (location = 9) in vec4 iSurfaceParams;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightViewProj;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec3 vAlbedo;
out float vRoughness;
out float vMetalness;
out vec3 vEmissiveColor;
out float vEmissiveIntensity;
out vec3 vLocalNormal;
out vec4 vSurfaceParams;
out vec4 vLightClipPos;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    vec4 worldPos4 = model * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;
    vAlbedo = iAlbedoRoughness.rgb;
    vRoughness = iAlbedoRoughness.a;
    vMetalness = iMetalnessParams.x;
    vEmissiveColor = iEmissiveColorStrength.rgb;
    vEmissiveIntensity = iEmissiveColorStrength.a;
    vLocalNormal = aNormal;
    vSurfaceParams = iSurfaceParams;
    vWorldNormal = mat3(model) * aNormal;
    vLightClipPos = uLightViewProj * worldPos4;

    gl_Position = uProj * uView * worldPos4;
}
)GLSL";

const char* kFragSrc = R"GLSL(#version 330 core

const float PI = 3.14159265359;
const int MAX_LOCAL_LIGHTS = 4;

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec3 vAlbedo;
in float vRoughness;
in float vMetalness;
in vec3 vEmissiveColor;
in float vEmissiveIntensity;
in vec3 vLocalNormal;
in vec4 vSurfaceParams;
in vec4 vLightClipPos;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uGroundColor;
uniform sampler2DShadow uShadowMap;
uniform vec2 uShadowTexelSize;
uniform int uLocalLightCount;
uniform vec4 uLocalLightPosRange[MAX_LOCAL_LIGHTS];
uniform vec4 uLocalLightColorIntensity[MAX_LOCAL_LIGHTS];

out vec4 FragColor;

float distributionGGX(float nDotH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PI * denom * denom, 1e-4);
}

float geometrySchlickGGX(float nDotV, float roughness) {
    float k = (roughness + 1.0);
    k = (k * k) / 8.0;
    return nDotV / mix(nDotV, 1.0, k);
}

float geometrySmith(float nDotV, float nDotL, float roughness) {
    return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float signedPattern(float value) {
    return sin(value);
}

float surfaceDetailSignal(int surfaceType, vec3 localNormal, float detailScale, float seed) {
    if (surfaceType == 1) {
        float coarse = signedPattern((localNormal.x * 5.8 + localNormal.z * 4.1 + seed * 6.2831) * detailScale);
        float fine = signedPattern((localNormal.y * 11.3 - localNormal.x * 3.4 + seed * 11.1) * detailScale);
        return clamp(0.5 + 0.28 * coarse + 0.18 * fine, 0.0, 1.0);
    }
    if (surfaceType == 2) {
        float bands = signedPattern(localNormal.y * detailScale * 16.0 + seed * 8.0);
        float swirl = signedPattern(localNormal.x * detailScale * 5.0 + localNormal.z * detailScale * 7.0 - seed * 5.0);
        return clamp(0.5 + 0.34 * bands + 0.12 * swirl, 0.0, 1.0);
    }
    if (surfaceType == 3) {
        float cells = signedPattern((localNormal.x + localNormal.z * 1.7 + seed * 1.9) * detailScale * 12.0);
        float flicker = signedPattern((localNormal.y * 1.6 - localNormal.x * 0.9 - seed * 2.7) * detailScale * 9.0);
        return clamp(0.52 + 0.26 * cells + 0.22 * flicker, 0.0, 1.0);
    }
    return 0.5;
}

void applySurfacePresentation(
    int surfaceType,
    float detailScale,
    float detailStrength,
    float seed,
    inout vec3 albedo,
    inout float roughness,
    inout vec3 emissive)
{
    if (surfaceType == 0 || detailStrength <= 0.0) {
        return;
    }

    float signal = surfaceDetailSignal(surfaceType, normalize(vLocalNormal), detailScale, seed);
    float signedSignal = signal * 2.0 - 1.0;
    if (surfaceType == 1) {
        albedo *= 1.0 + signedSignal * detailStrength;
        roughness = clamp(roughness + (0.12 - signedSignal * 0.20) * detailStrength, 0.08, 1.0);
        return;
    }
    if (surfaceType == 2) {
        vec3 darkBand = albedo * (1.0 - 0.55 * detailStrength);
        vec3 lightBand = mix(albedo, vec3(1.0), 0.22 * detailStrength);
        albedo = mix(darkBand, lightBand, signal);
        roughness = clamp(roughness + (0.18 - signedSignal * 0.24) * detailStrength, 0.08, 1.0);
        return;
    }
    if (surfaceType == 3) {
        albedo = mix(albedo * (1.0 - 0.10 * detailStrength), vec3(1.0, 0.92, 0.76), signal * 0.22 * detailStrength);
        emissive *= 1.0 + signedSignal * 0.55 * detailStrength;
        roughness = clamp(roughness + (0.08 - signal * 0.18) * detailStrength, 0.08, 1.0);
    }
}

float shadowVisibility(float nDotL) {
    if (nDotL <= 0.0 || uShadowTexelSize.x <= 0.0 || uShadowTexelSize.y <= 0.0) {
        return 1.0;
    }

    vec3 shadowNdc = vLightClipPos.xyz / vLightClipPos.w;
    vec3 shadowUv = shadowNdc * 0.5 + 0.5;
    if (shadowUv.z <= 0.0 || shadowUv.z >= 1.0 ||
        shadowUv.x <= 0.0 || shadowUv.x >= 1.0 ||
        shadowUv.y <= 0.0 || shadowUv.y >= 1.0)
    {
        return 1.0;
    }

    float bias = max(0.0015 * (1.0 - nDotL), 0.00045);
    return texture(uShadowMap, vec3(shadowUv.xy, shadowUv.z - bias));
}

vec3 localLightContribution(
    vec3 albedo,
    vec3 n,
    vec3 v,
    float nDotV,
    vec3 f0,
    float roughness,
    float metalness)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < uLocalLightCount; ++i) {
        vec3 toLight = uLocalLightPosRange[i].xyz - vWorldPos;
        float range = uLocalLightPosRange[i].w;
        float distanceSq = dot(toLight, toLight);
        float rangeSq = range * range;
        if (distanceSq <= 1e-8 || distanceSq >= rangeSq) {
            continue;
        }

        float invDistance = inversesqrt(distanceSq);
        vec3 l = toLight * invDistance;
        vec3 h = normalize(v + l);
        float nDotL = max(dot(n, l), 0.0);
        float nDotH = max(dot(n, h), 0.0);
        float vDotH = max(dot(v, h), 0.0);
        if (nDotL <= 0.0) {
            continue;
        }

        vec3 fresnel = fresnelSchlick(vDotH, f0);
        float distribution = distributionGGX(nDotH, roughness);
        float geometry = geometrySmith(nDotV, nDotL, roughness);
        vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotV * nDotL, 1e-4);
        vec3 kS = fresnel;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);

        float attenuation = max(1.0 - distanceSq / rangeSq, 0.0);
        attenuation *= attenuation;
        attenuation /= (1.0 + distanceSq * 0.02);

        vec3 lightRadiance = uLocalLightColorIntensity[i].rgb * attenuation;
        total += (kD * albedo / PI + specular) * lightRadiance * nDotL;
    }
    return total;
}

void main() {
    vec3 n = normalize(vWorldNormal);
    vec3 v = normalize(uCameraPos - vWorldPos);
    vec3 l = normalize(uLightDir);

    float nDotL = max(dot(n, l), 0.0);
    float nDotV = max(dot(n, v), 0.0);

    float roughness = clamp(vRoughness, 0.08, 1.0);
    float metalness = clamp(vMetalness, 0.0, 1.0);
    vec3 albedo = max(vAlbedo, vec3(0.0));
    vec3 emissive = vEmissiveColor * vEmissiveIntensity;
    applySurfacePresentation(
        int(floor(vSurfaceParams.x + 0.5)),
        max(vSurfaceParams.y, 0.0),
        max(vSurfaceParams.z, 0.0),
        vSurfaceParams.w,
        albedo,
        roughness,
        emissive);

    vec3 f0 = mix(vec3(0.04), albedo, metalness);
    vec3 ambientFresnel = fresnelSchlick(nDotV, f0);
    float visibility = 1.0;
    vec3 direct = vec3(0.0);
    if (nDotL > 0.0) {
        vec3 h = normalize(v + l);
        float nDotH = max(dot(n, h), 0.0);
        float vDotH = max(dot(v, h), 0.0);
        vec3 fresnel = fresnelSchlick(vDotH, f0);
        float distribution = distributionGGX(nDotH, roughness);
        float geometry = geometrySmith(nDotV, nDotL, roughness);
        vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotV * nDotL, 1e-4);
        vec3 kS = fresnel;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
        visibility = shadowVisibility(nDotL);
        direct = (kD * albedo / PI + specular) * uLightColor * nDotL * visibility;
    }

    vec3 localDirect = vec3(0.0);
    if (uLocalLightCount > 0) {
        localDirect = localLightContribution(albedo, n, v, nDotV, f0, roughness, metalness);
    }

    float skyMix = smoothstep(-0.18, 0.72, n.y);
    float zenithMix = smoothstep(0.08, 0.95, max(n.y, 0.0));
    vec3 upperAmbient = mix(uSkyHorizonColor, uSkyZenithColor, zenithMix);
    vec3 ambientLight = mix(uGroundColor, upperAmbient, skyMix);
    float horizonLift = 1.0 - smoothstep(0.05, 0.42, abs(n.y));
    ambientLight += uSkyHorizonColor * horizonLift * 0.05;
    float groundingOcclusion = mix(0.72, 1.0, visibility);
    vec3 ambientDiffuse = ambientLight * albedo * (1.0 - metalness * 0.7) * 0.38 * groundingOcclusion;
    vec3 ambientSpecular = ambientLight * ambientFresnel * mix(0.08, 0.22, 1.0 - roughness) * mix(0.78, 1.0, visibility);

    vec3 color = ambientDiffuse + ambientSpecular + direct + localDirect + emissive;
    FragColor = vec4(color, 1.0);
}
)GLSL";

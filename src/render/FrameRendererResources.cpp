#include "render/FrameRendererInternal.h"
#include "render/FrameRendererShaders.h"

namespace render_scene {

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

} // namespace render_scene

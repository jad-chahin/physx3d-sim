#include "render/FrameRendererInternal.h"

namespace render_scene {

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
        input.backdropPreset,
        input.sceneView->cameraPosition,
        input.sceneView->forward,
    };
    const bool lightingKeyChanged =
        !lightingCacheValid_ ||
        lightingCacheKey_.instanceRevision != lightingKey.instanceRevision ||
        lightingCacheKey_.shadowMapResolution != lightingKey.shadowMapResolution ||
        lightingCacheKey_.backdropPreset != lightingKey.backdropPreset ||
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
            shadow_.mapResolution,
            input.backdropPreset);
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

} // namespace render_scene

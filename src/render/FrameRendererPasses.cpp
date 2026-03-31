#include "render/FrameRendererInternal.h"

namespace render_scene {

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

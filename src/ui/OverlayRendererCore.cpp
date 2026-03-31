#include "ui/OverlayRendererInternal.h"

void OverlayRenderer::init()
{
    const GLuint vs = overlay_renderer_detail::compileShader(GL_VERTEX_SHADER, kUiVert);
    const GLuint fs = overlay_renderer_detail::compileShader(GL_FRAGMENT_SHADER, kUiFrag);
    program_ = overlay_renderer_detail::linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    uViewport_ = glGetUniformLocation(program_, "uViewport");
    uColor_ = glGetUniformLocation(program_, "uColor");

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &staticBuffer_.buffer);
    glGenBuffers(1, &dynamicBuffer_.buffer);
    staticBuffer_.uploadDirty = true;
    dynamicBuffer_.uploadDirty = true;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OverlayRenderer::shutdown()
{
    if (staticBuffer_.buffer) glDeleteBuffers(1, &staticBuffer_.buffer);
    if (dynamicBuffer_.buffer) glDeleteBuffers(1, &dynamicBuffer_.buffer);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    staticBuffer_ = {};
    dynamicBuffer_ = {};
    staticUploadScratch_.clear();
    dynamicUploadScratch_.clear();
    overlay_renderer::forEachSceneNode(retainedScene_, [](SceneNode& node) {
        node.uploadDirty = true;
        node.vertexOffset = 0;
        node.vertexCount = 0;
    });
    vao_ = 0;
    program_ = 0;
}

void OverlayRenderer::draw(
    int fbw,
    int fbh,
    bool simFrozen,
    double simSpeed,
    double simElapsed,
    double fps,
    const ui::MenuView& menu,
    const SpatialHud& spatialHud,
    const FocusMarker& focusMarker,
    const FocusHint& focusHint,
    const TargetHud& targetHud,
    float uiScale,
    bool showSimulationSpeed,
    bool showFps,
    bool showElapsedTime,
    bool showMinimap,
    bool showCoordinates,
    bool showCrosshair) const
{
    const FrameInput input{
        fbw,
        fbh,
        simFrozen,
        simSpeed,
        simElapsed,
        fps,
        &menu,
        &spatialHud,
        &focusMarker,
        &focusHint,
        &targetHud,
        uiScale,
        showSimulationSpeed,
        showFps,
        showElapsedTime,
        showMinimap,
        showCoordinates,
        showCrosshair,
    };
    RenderPassState renderState{};
    renderState.fbw = fbw;
    renderState.fbh = fbh;
    renderState.menuVisible = menu.visible;
    renderState.showHudSection = overlay_renderer::shouldShowHudSection(input);
    renderState.showCrosshair = showCrosshair;
    renderState.showTargetPopup = !menu.visible && targetHud.visible && !targetHud.lines.empty();
    renderState.showFrozenIndicator = simFrozen && !menu.visible;
    renderState.frozenOverlay = simFrozen;
    renderState.simFrozen = simFrozen;

    updateCachedLayout(input);
    uploadBatches();
    renderCachedLayout(renderState);
}

void OverlayRenderer::updateCachedLayout(const FrameInput& input) const
{
    const overlay_renderer::Geometry geometry{
        static_cast<float>(input.fbw),
        static_cast<float>(input.fbh),
        std::clamp(input.uiScale, 0.75f, 2.0f),
    };
    const GeometryKey geometryKey{input.fbw, input.fbh, geometry.uiScale};
    const overlay_renderer::Buffers buffers{
        retainedScene_.screenDim.geometry,
        retainedScene_.panelFill.geometry,
        retainedScene_.panelFrame.geometry,
        retainedScene_.accentFill.geometry,
        retainedScene_.textPrimary.geometry,
        retainedScene_.textMuted.geometry,
        retainedScene_.textAccent.geometry,
        retainedScene_.textWarning.geometry,
        retainedScene_.disabledOverlay.geometry,
        retainedScene_.statusText.geometry,
        retainedScene_.frozenIndicator.geometry,
        retainedScene_.crosshair.geometry,
        retainedScene_.popupBg.geometry,
        retainedScene_.popupFrame.geometry,
        retainedScene_.popupText.geometry,
    };

    if (input.menu->visible) {
        const MenuSectionState nextMenuState{geometryKey, *input.menu};
        if (!sceneCache_.menuValid || sceneCache_.menu != nextMenuState) {
            overlay_renderer::resetMenuSection(retainedScene_);
            overlay_renderer::drawMenu(*input.menu, geometry, buffers);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Static);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
            sceneCache_.menu = nextMenuState;
            sceneCache_.menuValid = true;
            sceneCache_.hudValid = false;
            sceneCache_.popupValid = false;
        }
    } else {
        const FocusMarker& focusMarker = overlay_renderer::resolveFocusMarker(input);
        const FocusHint& focusHint = overlay_renderer::resolveFocusHint(input);
        const bool showHudSection = overlay_renderer::shouldShowHudSection(input);
        if (showHudSection) {
            OverlayRenderer::SpatialHud cachedHudState = *input.spatialHud;
            cachedHudState.lookPitch = 0.0f;
            const HudSectionState nextHudState{
                geometryKey,
                input.showSimulationSpeed ? input.simSpeed : 0.0,
                input.showElapsedTime ? static_cast<int>(std::floor(std::max(0.0, input.simElapsed))) : 0,
                input.showFps ? input.fps : 0.0,
                cachedHudState,
                focusMarker,
                focusHint,
                input.showSimulationSpeed,
                input.showFps,
                input.showElapsedTime,
                input.showMinimap,
                input.showCoordinates,
            };
            if (!sceneCache_.hudValid || sceneCache_.hud != nextHudState) {
                overlay_renderer::resetHudSection(retainedScene_);
                overlay_renderer::drawHud(
                    geometry,
                    input.simSpeed,
                    input.simElapsed,
                    input.fps,
                    *input.spatialHud,
                    focusMarker,
                    focusHint,
                    input.showSimulationSpeed,
                    input.showFps,
                    input.showElapsedTime,
                    input.showMinimap,
                    input.showCoordinates,
                    buffers);
                overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Static);
                overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
                sceneCache_.hud = nextHudState;
                sceneCache_.hudValid = true;
                sceneCache_.menuValid = false;
            }
        } else if (sceneCache_.hudValid ||
            !retainedScene_.panelFill.geometry.empty() ||
            !retainedScene_.panelFrame.geometry.empty() ||
            !retainedScene_.textPrimary.geometry.empty() ||
            !retainedScene_.textMuted.geometry.empty() ||
            !retainedScene_.statusText.geometry.empty())
        {
            overlay_renderer::resetHudSection(retainedScene_);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Static);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
            sceneCache_.hudValid = false;
        }
        if (input.simFrozen) {
            const FrozenIndicatorSectionState nextFrozenIndicatorState{geometryKey};
            if (!sceneCache_.frozenIndicatorValid || sceneCache_.frozenIndicator != nextFrozenIndicatorState) {
                overlay_renderer::resetFrozenIndicatorSection(retainedScene_);
                overlay_renderer::drawFrozenIndicator(geometry, buffers);
                overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
                sceneCache_.frozenIndicator = nextFrozenIndicatorState;
                sceneCache_.frozenIndicatorValid = true;
            }
        } else if (sceneCache_.frozenIndicatorValid || !retainedScene_.frozenIndicator.geometry.empty()) {
            overlay_renderer::clearNodeGeometry(retainedScene_.frozenIndicator);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
            sceneCache_.frozenIndicatorValid = false;
        }
        if (input.showCrosshair) {
            const CrosshairSectionState nextCrosshairState{geometryKey, input.simFrozen};
            if (!sceneCache_.crosshairValid || sceneCache_.crosshair != nextCrosshairState) {
                overlay_renderer::resetCrosshairSection(retainedScene_);
                overlay_renderer::drawCrosshair(geometry, buffers);
                overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Static);
                sceneCache_.crosshair = nextCrosshairState;
                sceneCache_.crosshairValid = true;
            }
        } else if (sceneCache_.crosshairValid || !retainedScene_.crosshair.geometry.empty()) {
            overlay_renderer::clearNodeGeometry(retainedScene_.crosshair);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Static);
            sceneCache_.crosshairValid = false;
        }
        if (input.targetHud->visible && !input.targetHud->lines.empty()) {
            const PopupSectionState nextPopupState{geometryKey, *input.targetHud};
            if (!sceneCache_.popupValid || sceneCache_.popup != nextPopupState) {
                overlay_renderer::resetTargetPopupSection(retainedScene_);
                overlay_renderer::drawTargetPopup(*input.targetHud, geometry, buffers);
                overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
                sceneCache_.popup = nextPopupState;
                sceneCache_.popupValid = true;
                sceneCache_.menuValid = false;
            }
        } else if (sceneCache_.popupValid ||
            !retainedScene_.popupBg.geometry.empty() ||
            !retainedScene_.popupFrame.geometry.empty() ||
            !retainedScene_.popupText.geometry.empty())
        {
            overlay_renderer::clearNodeGeometry(retainedScene_.popupBg);
            overlay_renderer::clearNodeGeometry(retainedScene_.popupFrame);
            overlay_renderer::clearNodeGeometry(retainedScene_.popupText);
            overlay_renderer::markBatchDirty(staticBuffer_, dynamicBuffer_, BufferClass::Dynamic);
            sceneCache_.popupValid = false;
        }
    }
}

void OverlayRenderer::uploadBatches() const
{
    glUseProgram(program_);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    overlay_renderer::uploadBatch(
        retainedScene_,
        staticBuffer_,
        staticUploadScratch_,
        BufferClass::Static);
    overlay_renderer::uploadBatch(
        retainedScene_,
        dynamicBuffer_,
        dynamicUploadScratch_,
        BufferClass::Dynamic);
}

void OverlayRenderer::renderCachedLayout(const RenderPassState& renderState) const
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(uViewport_, static_cast<float>(renderState.fbw), static_cast<float>(renderState.fbh));
    glBindVertexArray(vao_);

    if (renderState.menuVisible) {
        drawLayer(
            vao_,
            staticBuffer_.buffer,
            retainedScene_.screenDim,
            uColor_,
            renderState.frozenOverlay ? kMenuDimFrozenColor : kMenuDimColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.panelFill, uColor_, kPanelFillMenuColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.panelFrame, uColor_, kPanelFrameColor);
        drawLayer(
            vao_,
            staticBuffer_.buffer,
            retainedScene_.accentFill,
            uColor_,
            renderState.frozenOverlay ? kAccentFrozenFillColor : kAccentFillColor);
        drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.textPrimary, uColor_, kTextPrimaryColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.textMuted, uColor_, kTextMutedColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.textAccent, uColor_, kTextAccentColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.textWarning, uColor_, kTextWarningColor);
        drawLayer(vao_, staticBuffer_.buffer, retainedScene_.disabledOverlay, uColor_, kDisabledOverlayColor);
        drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupBg, uColor_, kPopupBgColor);
        drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupFrame, uColor_, kPopupFrameColor);
        drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupText, uColor_, kPopupTextColor);
    } else {
        if (renderState.showHudSection) {
            drawLayer(vao_, staticBuffer_.buffer, retainedScene_.panelFill, uColor_, kPanelFillColor);
            drawLayer(vao_, staticBuffer_.buffer, retainedScene_.panelFrame, uColor_, kPanelFrameColor);
            drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.textPrimary, uColor_, kTextPrimaryColor);
            drawLayer(vao_, staticBuffer_.buffer, retainedScene_.textMuted, uColor_, kTextMutedColor);
            drawLayer(
                vao_,
                dynamicBuffer_.buffer,
                retainedScene_.statusText,
                uColor_,
                renderState.simFrozen ? kHudFrozenColor : kHudActiveColor);
        }
        if (renderState.showFrozenIndicator) {
            drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.frozenIndicator, uColor_, kHudFrozenColor);
        }
        if (renderState.showCrosshair) {
            drawLayer(
                vao_,
                staticBuffer_.buffer,
                retainedScene_.crosshair,
                uColor_,
                renderState.frozenOverlay
                    ? std::array<float, 4>{0.74f, 0.91f, 1.0f, 1.0f}
                    : std::array<float, 4>{0.32f, 0.85f, 0.95f, 1.0f});
        }
        if (renderState.showTargetPopup) {
            drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupBg, uColor_, kPopupBgColor);
            drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupFrame, uColor_, kPopupFrameColor);
            drawLayer(vao_, dynamicBuffer_.buffer, retainedScene_.popupText, uColor_, kPopupTextColor);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

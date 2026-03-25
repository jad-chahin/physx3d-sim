#include "ui/OverlayRenderer.h"
#include "ui/MinimapLayout.h"
#include "ui/OverlayRendererDrawShared.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {

GLuint compileShader(const GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint linkProgram(const GLuint vs, const GLuint fs) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    return program;
}

constexpr auto kUiVert = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPosPx;
uniform vec2 uViewport;
void main() {
    vec2 ndc;
    ndc.x = (aPosPx.x / uViewport.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (aPosPx.y / uViewport.y) * 2.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

constexpr auto kUiFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)GLSL";

constexpr std::array<float, 4> kMenuDimColor{0.01f, 0.015f, 0.028f, 0.48f};
constexpr std::array<float, 4> kMenuDimFrozenColor{0.02f, 0.028f, 0.046f, 0.70f};
constexpr std::array<float, 4> kPanelFillColor{0.045f, 0.055f, 0.078f, 0.90f};
constexpr std::array<float, 4> kPanelFillMenuColor{0.045f, 0.055f, 0.078f, 0.62f};
constexpr std::array<float, 4> kPanelFrameColor{0.26f, 0.31f, 0.39f, 0.92f};
constexpr std::array<float, 4> kAccentFillColor{0.44f, 0.56f, 0.69f, 0.92f};
constexpr std::array<float, 4> kAccentFrozenFillColor{0.58f, 0.69f, 0.82f, 0.92f};
constexpr std::array<float, 4> kTextPrimaryColor{0.93f, 0.95f, 0.98f, 1.0f};
constexpr std::array<float, 4> kTextMutedColor{0.68f, 0.72f, 0.79f, 0.98f};
constexpr std::array<float, 4> kTextAccentColor{0.78f, 0.85f, 0.92f, 1.0f};
constexpr std::array<float, 4> kTextWarningColor{0.94f, 0.55f, 0.38f, 1.0f};
constexpr std::array<float, 4> kDisabledOverlayColor{0.025f, 0.03f, 0.045f, 0.62f};
constexpr std::array<float, 4> kHudFrozenColor{0.80f, 0.87f, 0.96f, 1.0f};
constexpr std::array<float, 4> kHudActiveColor{0.56f, 0.86f, 0.60f, 1.0f};
constexpr std::array<float, 4> kPopupBgColor{0.05f, 0.06f, 0.085f, 1.0f};
constexpr std::array<float, 4> kPopupFrameColor{0.30f, 0.35f, 0.43f, 0.98f};
constexpr std::array<float, 4> kPopupTextColor{0.93f, 0.95f, 0.98f, 1.0f};

void drawLayer(
    const GLuint vao,
    const GLuint buffer,
    const OverlayRenderer::SceneNode& node,
    const GLint uColor,
    const std::array<float, 4>& color)
{
    if (node.vertexCount == 0) {
        return;
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);
    glDrawArrays(
        GL_TRIANGLES,
        static_cast<GLint>(node.vertexOffset),
        static_cast<GLsizei>(node.vertexCount));
}

} // namespace

namespace overlay_renderer {

struct Geometry {
    float width;
    float height;
    float uiScale;
};

struct Buffers {
    std::vector<float>& screenDim;
    std::vector<float>& panelFill;
    std::vector<float>& panelFrame;
    std::vector<float>& accentFill;
    std::vector<float>& textPrimary;
    std::vector<float>& textMuted;
    std::vector<float>& textAccent;
    std::vector<float>& textWarning;
    std::vector<float>& disabledOverlay;
    std::vector<float>& statusText;
    std::vector<float>& frozenIndicator;
    std::vector<float>& crosshair;
    std::vector<float>& popupBg;
    std::vector<float>& popupFrame;
    std::vector<float>& popupText;
};

void drawMenu(const ui::MenuView& menu, const Geometry& geometry, const Buffers& buffers);
void drawHud(
    const Geometry& geometry,
    double simSpeed,
    double simElapsed,
    double fps,
    const OverlayRenderer::SpatialHud& spatialHud,
    bool showSimulationSpeed,
    bool showFps,
    bool showElapsedTime,
    bool showMinimap,
    bool showCoordinates,
    const Buffers& buffers);
void drawFrozenIndicator(const Geometry& geometry, const Buffers& buffers);
void drawCrosshair(const Geometry& geometry, const Buffers& buffers);
void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers);

void resetNode(OverlayRenderer::SceneNode& node, const std::size_t reserveHint) {
    node.geometry.clear();
    node.uploadDirty = true;
    node.vertexOffset = 0;
    node.vertexCount = 0;
    if (node.geometry.capacity() < reserveHint) {
        node.geometry.reserve(reserveHint);
    }
}

void clearNodeGeometry(OverlayRenderer::SceneNode& node)
{
    if (node.geometry.empty() && node.vertexCount == 0) {
        return;
    }
    node.geometry.clear();
    node.uploadDirty = true;
    node.vertexOffset = 0;
    node.vertexCount = 0;
}

template <typename Fn>
void forEachSceneNode(OverlayRenderer::RetainedScene& scene, Fn&& fn) {
    fn(scene.screenDim);
    fn(scene.panelFill);
    fn(scene.panelFrame);
    fn(scene.accentFill);
    fn(scene.textPrimary);
    fn(scene.textMuted);
    fn(scene.textAccent);
    fn(scene.textWarning);
    fn(scene.disabledOverlay);
    fn(scene.statusText);
    fn(scene.frozenIndicator);
    fn(scene.crosshair);
    fn(scene.popupBg);
    fn(scene.popupFrame);
    fn(scene.popupText);
}

template <typename Fn>
void forEachSceneNode(const OverlayRenderer::RetainedScene& scene, Fn&& fn) {
    fn(scene.screenDim);
    fn(scene.panelFill);
    fn(scene.panelFrame);
    fn(scene.accentFill);
    fn(scene.textPrimary);
    fn(scene.textMuted);
    fn(scene.textAccent);
    fn(scene.textWarning);
    fn(scene.disabledOverlay);
    fn(scene.statusText);
    fn(scene.frozenIndicator);
    fn(scene.crosshair);
    fn(scene.popupBg);
    fn(scene.popupFrame);
    fn(scene.popupText);
}

OverlayRenderer::GpuBatchBuffer& selectBatchBuffer(
    OverlayRenderer::GpuBatchBuffer& staticBuffer,
    OverlayRenderer::GpuBatchBuffer& dynamicBuffer,
    const OverlayRenderer::BufferClass bufferClass)
{
    return bufferClass == OverlayRenderer::BufferClass::Static
        ? staticBuffer
        : dynamicBuffer;
}

void markBatchDirty(
    OverlayRenderer::GpuBatchBuffer& staticBuffer,
    OverlayRenderer::GpuBatchBuffer& dynamicBuffer,
    const OverlayRenderer::BufferClass bufferClass)
{
    selectBatchBuffer(staticBuffer, dynamicBuffer, bufferClass).uploadDirty = true;
}

void uploadBatch(
    OverlayRenderer::RetainedScene& scene,
    OverlayRenderer::GpuBatchBuffer& batchBuffer,
    std::vector<float>& scratch,
    const OverlayRenderer::BufferClass bufferClass)
{
    if (!batchBuffer.buffer || !batchBuffer.uploadDirty) {
        return;
    }

    scratch.clear();
    std::size_t vertexOffset = 0;
    forEachSceneNode(scene, [&](OverlayRenderer::SceneNode& node) {
        if (node.bufferClass != bufferClass) {
            return;
        }
        node.vertexOffset = vertexOffset;
        node.vertexCount = node.geometry.size() / 2;
        scratch.insert(scratch.end(), node.geometry.begin(), node.geometry.end());
        vertexOffset += node.vertexCount;
        node.uploadDirty = false;
    });

    glBindBuffer(GL_ARRAY_BUFFER, batchBuffer.buffer);
    const auto requiredFloats = scratch.size();
    if (requiredFloats > batchBuffer.capacityFloats) {
        batchBuffer.capacityFloats = std::max<std::size_t>(requiredFloats, batchBuffer.capacityFloats * 2 + 256);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(batchBuffer.capacityFloats * sizeof(float)),
            nullptr,
            bufferClass == OverlayRenderer::BufferClass::Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    }
    if (!scratch.empty()) {
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(requiredFloats * sizeof(float)),
            scratch.data());
    }
    batchBuffer.uploadDirty = false;
}

void resetMenuSection(OverlayRenderer::RetainedScene& scene) {
    resetNode(scene.screenDim, 24);
    resetNode(scene.panelFill, 600);
    resetNode(scene.panelFrame, 600);
    resetNode(scene.accentFill, 600);
    resetNode(scene.textPrimary, 9000);
    resetNode(scene.textMuted, 9000);
    resetNode(scene.textAccent, 9000);
    resetNode(scene.textWarning, 3000);
    resetNode(scene.disabledOverlay, 2400);
    resetNode(scene.popupBg, 600);
    resetNode(scene.popupFrame, 600);
    resetNode(scene.popupText, 1200);
    scene.statusText.geometry.clear();
    scene.statusText.uploadDirty = true;
}

void resetHudSection(OverlayRenderer::RetainedScene& scene) {
    resetNode(scene.panelFill, 600);
    resetNode(scene.panelFrame, 600);
    resetNode(scene.textPrimary, 3000);
    resetNode(scene.textMuted, 3000);
    resetNode(scene.statusText, 2000);
    clearNodeGeometry(scene.screenDim);
}

void resetFrozenIndicatorSection(OverlayRenderer::RetainedScene& scene) {
    resetNode(scene.frozenIndicator, 24);
}

void resetCrosshairSection(OverlayRenderer::RetainedScene& scene) {
    resetNode(scene.crosshair, 24);
}

void resetTargetPopupSection(OverlayRenderer::RetainedScene& scene) {
    resetNode(scene.popupBg, 600);
    resetNode(scene.popupFrame, 600);
    resetNode(scene.popupText, 1200);
}

} // namespace overlay_renderer

void OverlayRenderer::init() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kUiVert);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kUiFrag);
    program_ = linkProgram(vs, fs);
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

void OverlayRenderer::shutdown() {
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
    const TargetHud& targetHud,
    float uiScale,
    bool showSimulationSpeed,
    bool showFps,
    bool showElapsedTime,
    bool showMinimap,
    bool showCoordinates,
    bool showCrosshair) const
{
    const bool showHudSection =
        showSimulationSpeed || showFps || showElapsedTime || showMinimap || showCoordinates;
    const FrameInput input{
        fbw,
        fbh,
        simFrozen,
        simSpeed,
        simElapsed,
        fps,
        &menu,
        &spatialHud,
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
    renderState.showHudSection = showHudSection;
    renderState.showCrosshair = showCrosshair;
    renderState.showTargetPopup = !menu.visible && targetHud.visible && !targetHud.lines.empty();
    renderState.showFrozenIndicator = simFrozen && !menu.visible;
    renderState.frozenOverlay = simFrozen;
    renderState.simFrozen = simFrozen;

    updateCachedLayout(input);
    uploadBatches();
    renderCachedLayout(renderState);
}

void OverlayRenderer::updateCachedLayout(const FrameInput& input) const {
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
        const bool showHudSection =
            input.showSimulationSpeed || input.showFps ||
            input.showElapsedTime || input.showMinimap || input.showCoordinates;
        if (showHudSection) {
            OverlayRenderer::SpatialHud cachedHudState = *input.spatialHud;
            cachedHudState.lookPitch = 0.0f;
            const HudSectionState nextHudState{
                geometryKey,
                input.showSimulationSpeed ? input.simSpeed : 0.0,
                input.showElapsedTime ? static_cast<int>(std::floor(std::max(0.0, input.simElapsed))) : 0,
                input.showFps ? input.fps : 0.0,
                cachedHudState,
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

void OverlayRenderer::uploadBatches() const {
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

void OverlayRenderer::renderCachedLayout(const RenderPassState& renderState) const {
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

namespace overlay_renderer {
using namespace draw_shared;

namespace {

void pushTrianglePx(
    std::vector<float>& v,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float x2,
    const float y2)
{
    v.insert(v.end(), {x0, y0, x1, y1, x2, y2});
}

void pushDiscPx(
    std::vector<float>& v,
    const float cx,
    const float cy,
    const float radius,
    const int segments = 12)
{
    if (radius <= 0.0f || segments < 3) {
        return;
    }
    for (int i = 0; i < segments; ++i) {
        constexpr float kTau = 6.28318530717958647692f;
        const float a0 = kTau * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = kTau * static_cast<float>(i + 1) / static_cast<float>(segments);
        pushTrianglePx(
            v,
            cx,
            cy,
            cx + std::cos(a0) * radius,
            cy + std::sin(a0) * radius,
            cx + std::cos(a1) * radius,
            cy + std::sin(a1) * radius);
    }
}

const ButtonPalette& buttonPaletteForState(
    const ButtonStyle& style,
    const bool hovered,
    const bool selected)
{
    return selected ? style.selected : (hovered ? style.hover : style.normal);
}

void drawResetIconPx(
    std::vector<float>& out,
    const ui::pause_menu_layout::Rect& rect)
{
    const float width = rect.x1 - rect.x0;
    const float height = rect.y1 - rect.y0;
    const float cx = 0.5f * (rect.x0 + rect.x1);
    const float cy = 0.5f * (rect.y0 + rect.y1);
    const float radius = std::min(width, height) * 0.24f;
    const float thickness = std::max(1.4f, std::min(width, height) * 0.085f);
    constexpr int segments = 12;
    constexpr float kPi = 3.14159265359f;
    constexpr float startAngle = 0.55f * kPi;
    constexpr float endAngle = 2.08f * kPi;
    float prevX = cx + std::cos(startAngle) * radius;
    float prevY = cy + std::sin(startAngle) * radius;
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = startAngle + (endAngle - startAngle) * t;
        const float x = cx + std::cos(angle) * radius;
        const float y = cy + std::sin(angle) * radius;
        pushThickLinePx(out, prevX, prevY, x, y, thickness);
        prevX = x;
        prevY = y;
    }

    constexpr float headAngle = endAngle;
    const float tipX = cx + std::cos(headAngle) * radius;
    const float tipY = cy + std::sin(headAngle) * radius;
    const float tangentX = -std::sin(headAngle);
    const float tangentY = std::cos(headAngle);
    const float normalX = std::cos(headAngle);
    const float normalY = std::sin(headAngle);
    const float headLength = radius * 0.72f;
    const float headWidth = radius * 0.58f;
    pushTrianglePx(
        out,
        tipX,
        tipY,
        tipX - tangentX * headLength + normalX * headWidth * 0.5f,
        tipY - tangentY * headLength + normalY * headWidth * 0.5f,
        tipX - tangentX * headLength - normalX * headWidth * 0.5f,
        tipY - tangentY * headLength - normalY * headWidth * 0.5f);
}

void drawActionButtons(
    const ui::MenuView& menu,
    const ui::pause_menu_layout::PauseMenuLayout& layout,
    const ButtonStyle& style,
    const ui::ActionPlacement placement,
    const float padX,
    const float padY,
    const float textScalePx)
{
    for (std::size_t i = 0; i < menu.actions.size(); ++i) {
        const auto& action = menu.actions[i];
        if (action.placement != placement) {
            continue;
        }
        const auto& rect = layout.actionRect(i);
        drawButtonPx(style, rect, padX, padY, textScalePx, action.label, action.hovered, action.selected);
        if (action.id == static_cast<int>(ui::ActionKind::ResetSettings)) {
            drawResetIconPx(buttonPaletteForState(style, action.hovered, action.selected).text, rect);
        }
    }
}

} // namespace

void drawMenu(
    const ui::MenuView& menu,
    const Geometry& geometry,
    const Buffers& buffers)
{
    const float w = geometry.width;
    const float h = geometry.height;
    const ui::pause_menu_layout::PauseMenuLayout l =
        ui::pause_menu_layout::buildPauseMenuLayout(w, h, geometry.uiScale, menu);
    const float menuScale = l.menuScale;
    const float baseScalePx = l.baseScalePx;
    const float preferredTitleScalePx = kTitleScalePx * menuScale;

    pushQuadPx(buffers.screenDim, 0.0f, 0.0f, w, h);

    const float cardX0 = l.cardX0;
    const float cardY0 = l.cardY0;
    const float cardX1 = l.cardX1;
    const float cardY1 = l.cardY1;
    const float cardW = cardX1 - cardX0;
    pushQuadPx(buffers.panelFill, cardX0, cardY0, cardX1, cardY1);
    pushFramePx(buffers.panelFrame, cardX0, cardY0, cardX1, cardY1, 2.0f);

    const float centerX = w * 0.5f;
    const float infoWidth = cardW - 72.0f;
    const std::string title = menu.title.empty() ? "MENU" : menu.title;
    const std::string subtitle = menu.subtitle;
    const float titleScalePx = fitScaleForWidth(title, preferredTitleScalePx, infoWidth);
    const float subtitleScalePx =
        subtitle.empty() ? 0.0f : fitScaleForWidth(subtitle, baseScalePx * 1.05f, infoWidth);
    appendTextPx(
        buffers.textAccent,
        centerX - measureMaxLinePx(title, titleScalePx) * 0.5f,
        cardY0 + 18.0f * menuScale,
        titleScalePx,
        title);
    if (!subtitle.empty()) {
        appendTextPx(
            buffers.textMuted,
            centerX - measureMaxLinePx(subtitle, subtitleScalePx) * 0.5f,
            cardY0 + 64.0f * menuScale,
            subtitleScalePx,
            subtitle);
    }

    if (!menu.statusLine.empty()) {
        const float statusScalePx =
            fitScaleForWidth(menu.statusLine, baseScalePx * 0.95f, infoWidth);
        appendTextPx(
            menu.statusWarning ? buffers.textWarning : buffers.textAccent,
            centerX - measureMaxLinePx(menu.statusLine, statusScalePx) * 0.5f,
            cardY0 + 96.0f * menuScale,
            statusScalePx,
            menu.statusLine);
    }

    const float tabsScalePx = l.tabsScalePx;
    const float tabPadX = 12.0f * menuScale;
    const float tabPadY = 7.0f * menuScale;
    const ButtonStyle pauseButtonStyle{
        {buffers.panelFill, buffers.panelFrame, buffers.textMuted},
        {buffers.panelFrame, buffers.panelFrame, buffers.textPrimary},
        {buffers.panelFill, buffers.textPrimary, buffers.textPrimary},
    };
    const int maxTabIndex = static_cast<int>(menu.tabs.size()) - 1;
    const int activeTab = maxTabIndex >= 0 ? std::clamp(menu.activeTabIndex, 0, maxTabIndex) : -1;

    for (std::size_t i = 0; i < menu.tabs.size() && i < l.tabRects.size(); ++i) {
        drawButtonPx(
            pauseButtonStyle,
            l.tabRects[i],
            tabPadX,
            tabPadY,
            tabsScalePx,
            menu.tabs[i],
            menu.hoveredTabIndex == static_cast<int>(i),
            static_cast<int>(i) == activeTab);
    }

    drawActionButtons(
        menu,
        l,
        pauseButtonStyle,
        ui::ActionPlacement::Top,
        12.0f * menuScale,
        tabPadY,
        tabsScalePx);

    pushQuadPx(buffers.panelFill, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1);
    pushFramePx(buffers.panelFrame, l.sectionX0, l.settingsY0, l.sectionX1, l.settingsY1, 1.5f);
    const std::string sectionTitle = menu.sectionTitle.empty() ? "SETTINGS" : menu.sectionTitle;
    appendTextPx(
        buffers.textAccent,
        l.sectionX0 + 14.0f * menuScale,
        l.settingsY0 + 10.0f * menuScale,
        l.sectionHeaderScalePx,
        sectionTitle);

    if (l.visibleLines > 0 && l.totalLines > l.visibleLines) {
        const float trackW = 8.0f * menuScale;
        const float trackX1 = l.sectionX1 - 8.0f * menuScale;
        const float trackX0 = trackX1 - trackW;
        const float trackY0 = l.linesStartY;
        const float trackY1 = l.contentY1;
        pushQuadPx(buffers.panelFill, trackX0, trackY0, trackX1, trackY1);
        pushFramePx(buffers.panelFrame, trackX0, trackY0, trackX1, trackY1, 1.0f);
        const float visibleRatio =
            std::clamp(static_cast<float>(l.visibleLines) / static_cast<float>(l.totalLines), 0.0f, 1.0f);
        const float thumbH = std::max(18.0f * menuScale, (trackY1 - trackY0) * visibleRatio);
        const auto maxFirst = static_cast<float>(l.totalLines - l.visibleLines);
        const float scrollT = maxFirst > 0.0f ? static_cast<float>(l.firstLine) / maxFirst : 0.0f;
        const float thumbTravel = std::max(0.0f, (trackY1 - trackY0) - thumbH);
        const float thumbY0 = trackY0 + thumbTravel * scrollT;
        pushQuadPx(
            buffers.accentFill,
            trackX0 + 1.0f,
            thumbY0 + 1.0f,
            trackX1 - 1.0f,
            thumbY0 + thumbH - 1.0f);
    }

    drawActionButtons(
        menu,
        l,
        pauseButtonStyle,
        ui::ActionPlacement::Bottom,
        16.0f * menuScale,
        8.0f * menuScale,
        l.actionScalePx);

    for (std::size_t i = 0; i < l.visibleLines; ++i) {
        const std::size_t lineIdx = l.firstLine + i;
        const auto& line = menu.rows[lineIdx];
        const bool selected = line.selected;
        const bool hovered = static_cast<int>(lineIdx) == menu.hoveredRowIndex;
        const bool disabled = line.disabled;
        const auto widgets = l.lineWidgets(i, line);
        const float lineY = l.linesStartY + static_cast<float>(i) * l.rowStep;

        if (disabled && line.kind != ui::RowKind::Header) {
            pushQuadPx(
                buffers.disabledOverlay,
                widgets.line.x0,
                widgets.line.y0,
                widgets.line.x1,
                widgets.line.y1);
        } else if (selected && line.kind != ui::RowKind::Header) {
            pushFramePx(
                buffers.textPrimary,
                widgets.line.x0,
                widgets.line.y0,
                widgets.line.x1,
                widgets.line.y1,
                1.5f);
        } else if (hovered && line.kind != ui::RowKind::Header) {
            pushQuadPx(buffers.panelFrame, widgets.line.x0, widgets.line.y0, widgets.line.x1, widgets.line.y1);
        }

        const float lineScalePx = line.kind == ui::RowKind::Header
            ? fitScaleForWidth(line.label, l.rowScalePx * 1.03f, (l.sectionX1 - l.sectionX0))
            : l.rowScalePx;
        appendTextPx(
            line.kind == ui::RowKind::Header ? buffers.textAccent
                        : (disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted)),
            l.sectionX0 + 14.0f * menuScale,
            lineY,
            lineScalePx,
            line.label);
        if (line.kind == ui::RowKind::Header) {
            continue;
        }

        const float valueScalePx = l.rowScalePx * 0.88f;
        const float buttonY0 = lineY - 2.0f * menuScale;
        const float buttonY1 =
            lineY + (static_cast<float>(kFontH) + 3.5f) * l.rowScalePx + 2.0f * menuScale;
        const auto drawInlineButton =
            [&](const float x0, const float x1, const std::string& text, const bool active) {
                if (active && !disabled && !selected) {
                    pushQuadPx(buffers.panelFill, x0, buttonY0, x1, buttonY1);
                }
                if (active) {
                    pushFramePx(
                        disabled ? buffers.panelFrame : buffers.textPrimary,
                        x0,
                        buttonY0,
                        x1,
                        buttonY1,
                        1.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted
                             : (active ? buffers.textPrimary : buffers.textMuted),
                    x0 + (x1 - x0 - measureMaxLinePx(text, valueScalePx)) * 0.5f,
                    lineY + 0.3f * menuScale,
                    valueScalePx,
                    text);
            };

        switch (line.kind) {
            case ui::RowKind::Toggle: {
                const std::string toggleText = line.boolValue ? "ON" : "OFF";
                const float textX0 = widgets.checkbox.x1 + 5.0f * menuScale;
                const float textW = measureMaxLinePx(toggleText, valueScalePx);
                const float textX =
                    textX0 + std::max(0.0f, (widgets.value.x1 - 12.0f * menuScale - textX0 - textW) * 0.5f);
                pushFramePx(
                    buffers.textMuted,
                    widgets.checkbox.x0,
                    widgets.checkbox.y0,
                    widgets.checkbox.x1,
                    widgets.checkbox.y1,
                    1.1f);
                if (line.boolValue) {
                    pushQuadPx(
                        disabled ? buffers.panelFrame : buffers.accentFill,
                        widgets.checkbox.x0 + 2.0f,
                        widgets.checkbox.y0 + 2.0f,
                        widgets.checkbox.x1 - 2.0f,
                        widgets.checkbox.y1 - 2.0f);
                }
                appendTextPx(
                    disabled ? buffers.textMuted : (selected ? buffers.textPrimary : buffers.textMuted),
                    textX,
                    lineY + 3.3f * menuScale,
                    valueScalePx,
                    toggleText);
                break;
            }
            case ui::RowKind::Choice:
                drawInlineButton(widgets.leftArrow.x0, widgets.leftArrow.x1, "<", false);
                drawInlineButton(widgets.rightArrow.x0, widgets.rightArrow.x1, ">", false);
                drawInlineButton(widgets.value.x0, widgets.value.x1, line.value, false);
                break;
            case ui::RowKind::Rebind:
            case ui::RowKind::Header:
                if (!line.value.empty()) {
                    appendTextPx(
                        buffers.textMuted,
                        widgets.value.x0 +
                            ((widgets.value.x1 - widgets.value.x0) - measureMaxLinePx(line.value, valueScalePx)) *
                                0.5f,
                        lineY + 0.3f * menuScale,
                        valueScalePx,
                        line.value);
                }
                break;
        }
    }

    if (!menu.footerHint.empty()) {
        const float footerX0 = l.sectionX0;
        const float footerY0 = l.settingsY1 + 10.0f * menuScale;
        const float footerY1 = cardY1 - 16.0f * menuScale;
        pushQuadPx(buffers.panelFill, footerX0, footerY0, l.sectionX1, footerY1);
        pushFramePx(buffers.panelFrame, footerX0, footerY0, l.sectionX1, footerY1, 1.2f);
        appendTextPx(
            buffers.textMuted,
            footerX0 + 10.0f * menuScale,
            footerY0 + 6.0f * menuScale,
            fitScaleForWidth(
                menu.footerHint,
                baseScalePx * 0.92f,
                (l.sectionX1 - footerX0) - 18.0f * menuScale),
            menu.footerHint);
    }

    if (!menu.popup.visible) {
        return;
    }

    const auto popupButtons = ui::pause_menu_layout::buildPopupButtonsLayout(l, menu.popup);
    const std::string titleText =
        menu.popup.title.empty() ? "ARE YOU SURE?" : menu.popup.title;
    const std::string bodyText =
        menu.popup.body.empty() ? "CONFIRM ACTION" : menu.popup.body;

    pushQuadPx(buffers.popupBg, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1);
    pushFramePx(buffers.popupFrame, l.popupRect.x0, l.popupRect.y0, l.popupRect.x1, l.popupRect.y1, 1.5f);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 14.0f * menuScale,
        baseScalePx * 0.98f,
        titleText);
    appendTextPx(
        buffers.popupText,
        l.popupRect.x0 + 16.0f * menuScale,
        l.popupRect.y0 + 36.0f * menuScale,
        popupButtons.buttonScalePx,
        bodyText);

    if (menu.popup.hoverConfirm && !menu.popup.confirmSelected) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.yesButton.x0 + 1.0f,
            popupButtons.yesButton.y0 + 1.0f,
            popupButtons.yesButton.x1 - 1.0f,
            popupButtons.yesButton.y1 - 1.0f);
    }
    pushFramePx(
        menu.popup.confirmSelected ? buffers.popupText : buffers.popupFrame,
        popupButtons.yesButton.x0,
        popupButtons.yesButton.y0,
        popupButtons.yesButton.x1,
        popupButtons.yesButton.y1,
        menu.popup.confirmSelected ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.yesButton.x0 + popupButtons.buttonPadX,
        popupButtons.yesButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.yesLabel);

    const bool cancelSelected = !menu.popup.confirmSelected;
    if (menu.popup.hoverCancel && !cancelSelected) {
        pushQuadPx(
            buffers.popupFrame,
            popupButtons.noButton.x0 + 1.0f,
            popupButtons.noButton.y0 + 1.0f,
            popupButtons.noButton.x1 - 1.0f,
            popupButtons.noButton.y1 - 1.0f);
    }
    pushFramePx(
        cancelSelected ? buffers.popupText : buffers.popupFrame,
        popupButtons.noButton.x0,
        popupButtons.noButton.y0,
        popupButtons.noButton.x1,
        popupButtons.noButton.y1,
        cancelSelected ? 2.4f : 1.2f);
    appendTextPx(
        buffers.popupText,
        popupButtons.noButton.x0 + popupButtons.buttonPadX,
        popupButtons.noButton.y0 + popupButtons.buttonPadY,
        popupButtons.buttonScalePx,
        popupButtons.noLabel);
}

void drawHud(
    const Geometry& geometry,
    const double simSpeed,
    const double simElapsed,
    const double fps,
    const OverlayRenderer::SpatialHud& spatialHud,
    const bool showSimulationSpeed,
    const bool showFps,
    const bool showElapsedTime,
    const bool showMinimap,
    const bool showCoordinates,
    const Buffers& buffers)
{
    const float baseScalePx = kBaseScalePx * geometry.uiScale;
    const float hudRowStep = 20.0f * geometry.uiScale;
    constexpr float hudX0 = 16.0f;
    constexpr float hudY0 = 16.0f;
    constexpr float hudTextX = hudX0 + 10.0f;
    constexpr float hudTextY0 = hudY0 + 10.0f;
    char speedLine[64];
    char fpsLine[64];
    char timeLine[64];
    formatHudSpeed(speedLine, sizeof(speedLine), simSpeed);
    std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", fps);
    formatElapsedTime(timeLine, sizeof(timeLine), simElapsed);

    float infoMaxLineW = 0.0f;
    int infoLineCount = 0;
    if (showSimulationSpeed) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(speedLine, baseScalePx));
        ++infoLineCount;
    }
    if (showFps) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(fpsLine, baseScalePx));
        ++infoLineCount;
    }
    if (showElapsedTime) {
        infoMaxLineW = std::max(infoMaxLineW, measureMaxLinePx(timeLine, baseScalePx));
        ++infoLineCount;
    }

    const bool showInfoPanel = infoLineCount > 0;
    float infoPanelBottom = hudY0;
    if (showInfoPanel) {
        const float maxExpectedTimeWidth = measureMaxLinePx("TIME: 999D 23:59:59", baseScalePx);
        const float panelTextWidth = std::max(infoMaxLineW, maxExpectedTimeWidth);
        const float hudX1 = std::min(geometry.width - 16.0f, hudTextX + panelTextWidth + 20.0f);
        const float hudY1 = hudY0 + 16.0f + static_cast<float>(infoLineCount) * hudRowStep;
        pushQuadPx(buffers.panelFill, hudX0, hudY0, hudX1, hudY1);
        pushFramePx(buffers.panelFrame, hudX0, hudY0, hudX1, hudY1, 1.5f);

        float lineY = hudTextY0;
        if (showSimulationSpeed) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, speedLine);
            lineY += hudRowStep;
        }
        if (showFps) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, fpsLine);
            lineY += hudRowStep;
        }
        if (showElapsedTime) {
            appendTextPx(buffers.textPrimary, hudTextX, lineY, baseScalePx, timeLine);
        }
        infoPanelBottom = hudY1;
    }

    const float coordsScalePx = baseScalePx * 0.9f;
    if (showCoordinates) {
        char coordXLine[64];
        char coordYLine[64];
        char coordZLine[64];
        std::snprintf(coordXLine, sizeof(coordXLine), "X: %.2f", spatialHud.worldX);
        std::snprintf(coordYLine, sizeof(coordYLine), "Y: %.2f", spatialHud.worldY);
        std::snprintf(coordZLine, sizeof(coordZLine), "Z: %.2f", spatialHud.worldZ);

        constexpr float coordsX0 = hudX0;
        const float coordsY0 = showInfoPanel ? (infoPanelBottom + 14.0f * geometry.uiScale) : hudY0;
        constexpr float coordsTextX = coordsX0 + 10.0f;
        const float coordsTextY = coordsY0 + 9.0f;
        const float coordsWidth = std::max(
            {measureMaxLinePx(coordXLine, coordsScalePx),
             measureMaxLinePx(coordYLine, coordsScalePx),
             measureMaxLinePx(coordZLine, coordsScalePx)}) + 20.0f;
        const float coordsX1 = coordsX0 + coordsWidth + 10.0f;
        const float coordsY1 = coordsY0 + 12.0f + 3.0f * 18.0f * geometry.uiScale;
        pushQuadPx(buffers.panelFill, coordsX0, coordsY0, coordsX1, coordsY1);
        pushFramePx(buffers.panelFrame, coordsX0, coordsY0, coordsX1, coordsY1, 1.2f);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY, coordsScalePx, coordXLine);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY + 18.0f * geometry.uiScale, coordsScalePx, coordYLine);
        appendTextPx(buffers.textPrimary, coordsTextX, coordsTextY + 36.0f * geometry.uiScale, coordsScalePx, coordZLine);
    }

    if (showMinimap) {
        const float mapSize = 148.0f * geometry.uiScale;
        const float mapMargin = 16.0f * geometry.uiScale;
        const float mapX1 = geometry.width - mapMargin;
        const float mapY1 = geometry.height - mapMargin;
        const float mapX0 = mapX1 - mapSize;
        const float mapY0 = mapY1 - mapSize;
        const float mapCenterX = (mapX0 + mapX1) * 0.5f;
        const float mapCenterY = (mapY0 + mapY1) * 0.5f;
        const float mapInnerPad = 10.0f * geometry.uiScale;
        const float mapInnerX0 = mapX0 + mapInnerPad;
        const float mapInnerY0 = mapY0 + mapInnerPad;
        const float mapInnerX1 = mapX1 - mapInnerPad;
        const float mapInnerY1 = mapY1 - mapInnerPad;
        const float mapHalfSpanX = (mapInnerX1 - mapInnerX0) * 0.5f;
        const float mapHalfSpanY = (mapInnerY1 - mapInnerY0) * 0.5f;
        pushQuadPx(buffers.panelFill, mapX0, mapY0, mapX1, mapY1);
        pushFramePx(buffers.panelFrame, mapX0, mapY0, mapX1, mapY1, 1.5f);
        const float forwardLen = 10.0f * geometry.uiScale;
        const float baseBack = 5.0f * geometry.uiScale;
        const float halfWidth = 4.0f * geometry.uiScale;
        const float dirX = spatialHud.forwardX;
        const float dirY = spatialHud.forwardY;
        const float sideX = -dirY;
        const float sideY = dirX;
        const float tipX = mapCenterX + dirX * forwardLen;
        const float tipY = mapCenterY + dirY * forwardLen;
        const float baseX = mapCenterX - dirX * baseBack;
        const float baseY = mapCenterY - dirY * baseBack;
        const float leftX = baseX + sideX * halfWidth;
        const float leftY = baseY + sideY * halfWidth;
        const float rightX = baseX - sideX * halfWidth;
        const float rightY = baseY - sideY * halfWidth;
        pushTrianglePx(buffers.statusText, tipX, tipY, leftX, leftY, rightX, rightY);
        pushThickLinePx(
            buffers.panelFrame,
            leftX,
            leftY,
            tipX,
            tipY,
            std::max(1.0f, geometry.uiScale));
        pushThickLinePx(
            buffers.panelFrame,
            rightX,
            rightY,
            tipX,
            tipY,
            std::max(1.0f, geometry.uiScale));
        pushThickLinePx(
            buffers.panelFrame,
            leftX,
            leftY,
            rightX,
            rightY,
            std::max(1.0f, geometry.uiScale));
        for (const auto& marker : spatialHud.markers) {
            const float projectedX = mapCenterX + marker.xNorm * mapHalfSpanX;
            const float projectedY = mapCenterY + marker.yNorm * mapHalfSpanY;
            const float markerClampPad = 6.0f * geometry.uiScale;
            const ui::minimap::MarkerLayout markerLayout = ui::minimap::computeMarkerLayout(
                projectedX,
                projectedY,
                mapInnerX0,
                mapInnerY0,
                mapInnerX1,
                mapInnerY1,
                markerClampPad,
                marker.heightNorm,
                marker.edgeClamped);
            std::vector<float>& markerBuffer =
                marker.highlighted ? buffers.statusText : (marker.dynamicBody ? buffers.textPrimary : buffers.textMuted);
            float markerX = markerLayout.markerX;
            float markerY = markerLayout.markerY;
            float markerRadius = (marker.dynamicBody ? 2.7f : 2.1f) * geometry.uiScale;
            if (marker.highlighted) {
                const float outerRadius = 5.0f * geometry.uiScale;
                const float innerRadius = 2.5f * geometry.uiScale;
                markerRadius = outerRadius;
                if (markerLayout.showHeightCue) {
                    markerY = ui::minimap::clampMarkerYForCue(
                        markerY,
                        mapInnerY0,
                        mapInnerY1,
                        markerRadius,
                        2.0f * geometry.uiScale,
                        3.6f * geometry.uiScale,
                        marker.aboveCamera);
                }
                pushDiscPx(buffers.statusText, markerX, markerY, outerRadius);
                pushDiscPx(buffers.panelFill, markerX, markerY, innerRadius);
            } else {
                if (markerLayout.visualEdgeClamped) {
                    markerRadius = (marker.dynamicBody ? 2.9f : 2.3f) * geometry.uiScale;
                }
                if (markerLayout.showHeightCue) {
                    markerY = ui::minimap::clampMarkerYForCue(
                        markerY,
                        mapInnerY0,
                        mapInnerY1,
                        markerRadius,
                        2.0f * geometry.uiScale,
                        3.6f * geometry.uiScale,
                        marker.aboveCamera);
                }
                pushDiscPx(markerBuffer, markerX, markerY, markerRadius);
            }

            if (markerLayout.showHeightCue) {
                const float symbolGap = 2.0f * geometry.uiScale;
                const float symbolHalfWidth = 2.2f * geometry.uiScale;
                const float symbolHeight = 3.6f * geometry.uiScale;
                const float symbolCenterY = std::clamp(
                    markerY + (marker.aboveCamera ? -(markerRadius + symbolGap + symbolHeight * 0.5f)
                                                  : (markerRadius + symbolGap + symbolHeight * 0.5f)),
                    mapInnerY0 + symbolHeight * 0.5f,
                    mapInnerY1 - symbolHeight * 0.5f);
                const float symbolTipY = marker.aboveCamera
                    ? symbolCenterY - symbolHeight * 0.5f
                    : symbolCenterY + symbolHeight * 0.5f;
                const float symbolBaseY = marker.aboveCamera
                    ? symbolCenterY + symbolHeight * 0.5f
                    : symbolCenterY - symbolHeight * 0.5f;
                pushTrianglePx(
                    markerBuffer,
                    markerX,
                    symbolTipY,
                    markerX - symbolHalfWidth,
                    symbolBaseY,
                    markerX + symbolHalfWidth,
                    symbolBaseY);
            }
        }
    }
}

void drawFrozenIndicator(const Geometry& geometry, const Buffers& buffers) {
    constexpr float indicatorInset = 16.0f;
    const float baseScalePx = kBaseScalePx * geometry.uiScale * 0.92f;
    const char* label = "FROZEN";
    const float labelWidth = measureLinePx(label, baseScalePx);
    const float iconH = 26.0f * geometry.uiScale;
    const float iconW = 20.0f * geometry.uiScale;
    const float labelGap = 10.0f * geometry.uiScale;
    const float x1 = geometry.width - indicatorInset * geometry.uiScale;
    const float x0 = x1 - iconW;
    const float y0 = indicatorInset * geometry.uiScale;
    const float y1 = y0 + iconH;
    const float barW = std::max(4.0f, iconW * 0.24f);
    const float gap = std::max(4.5f, iconW * 0.20f);
    const float totalBarsW = barW * 2.0f + gap;
    const float leftX0 = x0 + (iconW - totalBarsW) * 0.5f;
    const float leftX1 = leftX0 + barW;
    const float rightX0 = leftX1 + gap;
    const float rightX1 = rightX0 + barW;
    const float barY0 = y0 + iconH * 0.08f;
    const float barY1 = y1 - iconH * 0.08f;
    const float labelX = x0 - labelGap - labelWidth;
    const float labelY = y0 + std::max(0.0f, (iconH - static_cast<float>(kFontH) * baseScalePx) * 0.5f);
    appendTextPx(buffers.frozenIndicator, labelX, labelY, baseScalePx, label);
    pushQuadPx(buffers.frozenIndicator, leftX0, barY0, leftX1, barY1);
    pushQuadPx(buffers.frozenIndicator, rightX0, barY0, rightX1, barY1);
}

void drawCrosshair(const Geometry& geometry, const Buffers& buffers) {
    const float cx = geometry.width * 0.5f;
    const float cy = geometry.height * 0.5f;
    pushQuadPx(buffers.crosshair, cx - 13.0f, cy - 1.0f, cx - 5.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx + 5.0f, cy - 1.0f, cx + 13.0f, cy + 1.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 13.0f, cx + 1.0f, cy - 5.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy + 5.0f, cx + 1.0f, cy + 13.0f);
    pushQuadPx(buffers.crosshair, cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f);
}

void drawTargetPopup(
    const OverlayRenderer::TargetHud& targetHud,
    const Geometry& geometry,
    const Buffers& buffers)
{
    if (!targetHud.visible || targetHud.lines.empty()) {
        return;
    }

    const float baseScalePx = kBaseScalePx * geometry.uiScale * 0.88f;
    float maxLineW = 0.0f;
    std::string popup;
    popup.reserve(192);
    for (std::size_t i = 0; i < targetHud.lines.size(); ++i) {
        if (i > 0) {
            popup += '\n';
        }
        popup += targetHud.lines[i];
        maxLineW = std::max(maxLineW, measureLinePx(targetHud.lines[i], baseScalePx));
    }

    const float popupW = maxLineW + 24.0f;
    const float popupH =
        static_cast<float>((kFontH + 2) * static_cast<int>(targetHud.lines.size())) * baseScalePx +
        12.0f;
    const float px = targetHud.xPx - popupW * 0.5f;
    const float py = targetHud.yPx - popupH - 12.0f;
    pushQuadPx(buffers.popupBg, px, py, px + popupW, py + popupH);
    pushFramePx(buffers.popupFrame, px, py, px + popupW, py + popupH, 1.5f);
    appendTextPx(buffers.popupText, px + 8.0f, py + 6.0f, baseScalePx, popup);
}

} // namespace overlay_renderer

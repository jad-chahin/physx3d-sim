#ifndef PHYSICS3D_UI_OVERLAYRENDERERINTERNAL_H
#define PHYSICS3D_UI_OVERLAYRENDERERINTERNAL_H

#include "ui/OverlayRenderer.h"
#include "ui/MinimapLayout.h"
#include "ui/OverlayRendererDrawShared.h"

#include <algorithm>
#include <array>
#include <vector>

namespace overlay_renderer_detail {

inline GLuint compileShader(const GLenum type, const char* src)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

inline GLuint linkProgram(const GLuint vs, const GLuint fs)
{
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    return program;
}

} // namespace overlay_renderer_detail

inline constexpr auto kUiVert = R"GLSL(
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

inline constexpr auto kUiFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)GLSL";

inline constexpr std::array<float, 4> kMenuDimColor{0.01f, 0.015f, 0.028f, 0.48f};
inline constexpr std::array<float, 4> kMenuDimFrozenColor{0.02f, 0.028f, 0.046f, 0.70f};
inline constexpr std::array<float, 4> kPanelFillColor{0.045f, 0.055f, 0.078f, 0.90f};
inline constexpr std::array<float, 4> kPanelFillMenuColor{0.045f, 0.055f, 0.078f, 0.62f};
inline constexpr std::array<float, 4> kPanelFrameColor{0.26f, 0.31f, 0.39f, 0.92f};
inline constexpr std::array<float, 4> kAccentFillColor{0.44f, 0.56f, 0.69f, 0.92f};
inline constexpr std::array<float, 4> kAccentFrozenFillColor{0.58f, 0.69f, 0.82f, 0.92f};
inline constexpr std::array<float, 4> kTextPrimaryColor{0.93f, 0.95f, 0.98f, 1.0f};
inline constexpr std::array<float, 4> kTextMutedColor{0.68f, 0.72f, 0.79f, 0.98f};
inline constexpr std::array<float, 4> kTextAccentColor{0.78f, 0.85f, 0.92f, 1.0f};
inline constexpr std::array<float, 4> kTextWarningColor{0.94f, 0.55f, 0.38f, 1.0f};
inline constexpr std::array<float, 4> kDisabledOverlayColor{0.025f, 0.03f, 0.045f, 0.62f};
inline constexpr std::array<float, 4> kHudFrozenColor{0.80f, 0.87f, 0.96f, 1.0f};
inline constexpr std::array<float, 4> kHudActiveColor{0.56f, 0.86f, 0.60f, 1.0f};
inline constexpr std::array<float, 4> kPopupBgColor{0.05f, 0.06f, 0.085f, 1.0f};
inline constexpr std::array<float, 4> kPopupFrameColor{0.30f, 0.35f, 0.43f, 0.98f};
inline constexpr std::array<float, 4> kPopupTextColor{0.93f, 0.95f, 0.98f, 1.0f};

inline void drawLayer(
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
    const OverlayRenderer::FocusMarker& focusMarker,
    const OverlayRenderer::FocusHint& focusHint,
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
void drawFocusMarker(
    const OverlayRenderer::FocusMarker& focusMarker,
    const Geometry& geometry,
    const Buffers& buffers);
void drawFocusHint(
    const OverlayRenderer::FocusHint& focusHint,
    const Geometry& geometry,
    const Buffers& buffers,
    float baseScalePx);

[[nodiscard]] inline const OverlayRenderer::FocusMarker& resolveFocusMarker(const OverlayRenderer::FrameInput& input)
{
    static const OverlayRenderer::FocusMarker kEmptyFocusMarker{};
    return input.focusMarker != nullptr ? *input.focusMarker : kEmptyFocusMarker;
}

[[nodiscard]] inline const OverlayRenderer::FocusHint& resolveFocusHint(const OverlayRenderer::FrameInput& input)
{
    static const OverlayRenderer::FocusHint kEmptyFocusHint{};
    return input.focusHint != nullptr ? *input.focusHint : kEmptyFocusHint;
}

[[nodiscard]] inline bool shouldShowHudSection(const OverlayRenderer::FrameInput& input)
{
    const auto& focusMarker = resolveFocusMarker(input);
    const auto& focusHint = resolveFocusHint(input);
    return input.showSimulationSpeed || input.showFps || input.showElapsedTime || input.showMinimap ||
        input.showCoordinates || focusMarker.visible || focusHint.visible;
}

inline void resetNode(OverlayRenderer::SceneNode& node, const std::size_t reserveHint)
{
    node.geometry.clear();
    node.uploadDirty = true;
    node.vertexOffset = 0;
    node.vertexCount = 0;
    if (node.geometry.capacity() < reserveHint) {
        node.geometry.reserve(reserveHint);
    }
}

inline void clearNodeGeometry(OverlayRenderer::SceneNode& node)
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
void forEachSceneNode(OverlayRenderer::RetainedScene& scene, Fn&& fn)
{
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
void forEachSceneNode(const OverlayRenderer::RetainedScene& scene, Fn&& fn)
{
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

inline OverlayRenderer::GpuBatchBuffer& selectBatchBuffer(
    OverlayRenderer::GpuBatchBuffer& staticBuffer,
    OverlayRenderer::GpuBatchBuffer& dynamicBuffer,
    const OverlayRenderer::BufferClass bufferClass)
{
    return bufferClass == OverlayRenderer::BufferClass::Static
        ? staticBuffer
        : dynamicBuffer;
}

inline void markBatchDirty(
    OverlayRenderer::GpuBatchBuffer& staticBuffer,
    OverlayRenderer::GpuBatchBuffer& dynamicBuffer,
    const OverlayRenderer::BufferClass bufferClass)
{
    selectBatchBuffer(staticBuffer, dynamicBuffer, bufferClass).uploadDirty = true;
}

inline void uploadBatch(
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

inline void resetMenuSection(OverlayRenderer::RetainedScene& scene)
{
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

inline void resetHudSection(OverlayRenderer::RetainedScene& scene)
{
    resetNode(scene.panelFill, 600);
    resetNode(scene.panelFrame, 600);
    resetNode(scene.textPrimary, 3000);
    resetNode(scene.textMuted, 3000);
    resetNode(scene.statusText, 2000);
    clearNodeGeometry(scene.screenDim);
}

inline void resetFrozenIndicatorSection(OverlayRenderer::RetainedScene& scene)
{
    resetNode(scene.frozenIndicator, 24);
}

inline void resetCrosshairSection(OverlayRenderer::RetainedScene& scene)
{
    resetNode(scene.crosshair, 24);
}

inline void resetTargetPopupSection(OverlayRenderer::RetainedScene& scene)
{
    resetNode(scene.popupBg, 600);
    resetNode(scene.popupFrame, 600);
    resetNode(scene.popupText, 1200);
}

} // namespace overlay_renderer

#endif // PHYSICS3D_UI_OVERLAYRENDERERINTERNAL_H

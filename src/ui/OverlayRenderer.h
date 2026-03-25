#ifndef PHYSICS3D_UI_OVERLAYRENDERER_H
#define PHYSICS3D_UI_OVERLAYRENDERER_H

#include "ui/PauseMenu.h"

#include <glad/glad.h>
#include <string>
#include <vector>

class OverlayRenderer {
public:
    struct TargetHud {
        bool visible = false;
        float xPx = 0.0f;
        float yPx = 0.0f;
        std::vector<std::string> lines;
        bool operator==(const TargetHud&) const = default;
    };

    struct MinimapMarker {
        float xNorm = 0.0f;
        float yNorm = 0.0f;
        float heightNorm = 0.0f;
        bool highlighted = false;
        bool dynamicBody = false;
        bool aboveCamera = false;
        bool edgeClamped = false;
        bool operator==(const MinimapMarker&) const = default;
    };

    struct SpatialHud {
        float worldX = 0.0f;
        float worldY = 0.0f;
        float worldZ = 0.0f;
        float rangeMeters = 40.0f;
        float forwardX = 0.0f;
        float forwardY = -1.0f;
        float lookPitch = 0.0f;
        std::vector<MinimapMarker> markers;
        bool operator==(const SpatialHud&) const = default;
    };

    void init();
    void shutdown();
    void draw(
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
        bool showCrosshair) const;

    enum class SceneLayer {
        ScreenDim,
        PanelFill,
        PanelFrame,
        AccentFill,
        TextPrimary,
        TextMuted,
        TextAccent,
        TextWarning,
        DisabledOverlay,
        StatusText,
        FrozenIndicator,
        Crosshair,
        PopupBg,
        PopupFrame,
        PopupText,
    };

    enum class BufferClass {
        Static,
        Dynamic,
    };

    struct SceneNode {
        SceneLayer layer;
        BufferClass bufferClass;
        std::vector<float> geometry;
        bool uploadDirty = true;
        std::size_t vertexOffset = 0;
        std::size_t vertexCount = 0;
    };

    struct RetainedScene {
        SceneNode screenDim{SceneLayer::ScreenDim, BufferClass::Static, {}};
        SceneNode panelFill{SceneLayer::PanelFill, BufferClass::Static, {}};
        SceneNode panelFrame{SceneLayer::PanelFrame, BufferClass::Static, {}};
        SceneNode accentFill{SceneLayer::AccentFill, BufferClass::Static, {}};
        SceneNode textPrimary{SceneLayer::TextPrimary, BufferClass::Dynamic, {}};
        SceneNode textMuted{SceneLayer::TextMuted, BufferClass::Static, {}};
        SceneNode textAccent{SceneLayer::TextAccent, BufferClass::Static, {}};
        SceneNode textWarning{SceneLayer::TextWarning, BufferClass::Static, {}};
        SceneNode disabledOverlay{SceneLayer::DisabledOverlay, BufferClass::Static, {}};
        SceneNode statusText{SceneLayer::StatusText, BufferClass::Dynamic, {}};
        SceneNode frozenIndicator{SceneLayer::FrozenIndicator, BufferClass::Dynamic, {}};
        SceneNode crosshair{SceneLayer::Crosshair, BufferClass::Static, {}};
        SceneNode popupBg{SceneLayer::PopupBg, BufferClass::Dynamic, {}};
        SceneNode popupFrame{SceneLayer::PopupFrame, BufferClass::Dynamic, {}};
        SceneNode popupText{SceneLayer::PopupText, BufferClass::Dynamic, {}};
    };

    struct GeometryKey {
        int fbw = 0;
        int fbh = 0;
        float uiScale = 1.0f;
        bool operator==(const GeometryKey&) const = default;
    };

    struct MenuSectionState {
        GeometryKey geometry{};
        ui::MenuView menu{};
        bool operator==(const MenuSectionState&) const = default;
    };

    struct HudSectionState {
        GeometryKey geometry{};
        double simSpeed = 1.0;
        int elapsedSeconds = 0;
        double fps = 0.0;
        SpatialHud spatialHud{};
        bool showSimulationSpeed = false;
        bool showFps = false;
        bool showElapsedTime = false;
        bool showMinimap = false;
        bool showCoordinates = false;
        bool operator==(const HudSectionState&) const = default;
    };

    struct FrozenIndicatorSectionState {
        GeometryKey geometry{};
        bool operator==(const FrozenIndicatorSectionState&) const = default;
    };

    struct CrosshairSectionState {
        GeometryKey geometry{};
        bool simFrozen = false;
        bool operator==(const CrosshairSectionState&) const = default;
    };
    struct PopupSectionState {
        GeometryKey geometry{};
        TargetHud targetHud{};
        bool operator==(const PopupSectionState&) const = default;
    };

    struct SceneCache {
        bool menuValid = false;
        bool hudValid = false;
        bool frozenIndicatorValid = false;
        bool crosshairValid = false;
        bool popupValid = false;
        MenuSectionState menu{};
        HudSectionState hud{};
        FrozenIndicatorSectionState frozenIndicator{};
        CrosshairSectionState crosshair{};
        PopupSectionState popup{};
    };

    struct GpuBatchBuffer {
        GLuint buffer = 0;
        std::size_t capacityFloats = 0;
        bool uploadDirty = true;
    };

    struct FrameInput {
        int fbw = 0;
        int fbh = 0;
        bool simFrozen = false;
        double simSpeed = 1.0;
        double simElapsed = 0.0;
        double fps = 0.0;
        const ui::MenuView* menu = nullptr;
        const SpatialHud* spatialHud = nullptr;
        const TargetHud* targetHud = nullptr;
        float uiScale = 1.0f;
        bool showSimulationSpeed = false;
        bool showFps = false;
        bool showElapsedTime = false;
        bool showMinimap = false;
        bool showCoordinates = false;
        bool showCrosshair = false;
    };

    struct RenderPassState {
        int fbw = 0;
        int fbh = 0;
        bool menuVisible = false;
        bool showHudSection = false;
        bool showCrosshair = false;
        bool showTargetPopup = false;
        bool showFrozenIndicator = false;
        bool frozenOverlay = false;
        bool simFrozen = false;
    };

private:
    void updateCachedLayout(const FrameInput& input) const;
    void uploadBatches() const;
    void renderCachedLayout(const RenderPassState& renderState) const;

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLint uViewport_ = -1;
    GLint uColor_ = -1;
    mutable RetainedScene retainedScene_{};
    mutable SceneCache sceneCache_{};
    mutable GpuBatchBuffer staticBuffer_{};
    mutable GpuBatchBuffer dynamicBuffer_{};
    mutable std::vector<float> staticUploadScratch_{};
    mutable std::vector<float> dynamicUploadScratch_{};
};

#endif // PHYSICS3D_UI_OVERLAYRENDERER_H

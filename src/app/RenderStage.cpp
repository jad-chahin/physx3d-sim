#include "app/RenderStage.h"

#include <string>

#include "app/ScenePresentation.h"
#include "app/SceneSnapshot.h"

namespace app_loop {
namespace {

render_scene::BackdropPreset selectedBackdropPreset(const ui::InterfaceSettings& interfaceSettings)
{
    return interfaceSettings.backdropPresetIndex == 1
        ? render_scene::BackdropPreset::Space
        : render_scene::BackdropPreset::Sunny;
}

} // namespace

void renderStage(
    GLFWwindow* window,
    const AppLoopState& appState,
    const RuntimeState& runtime,
    const ui::PauseMenu& pauseMenu,
    const input::ControlBindings& controls,
    const SimulationController& simulation,
    const input::Camera& cam,
    render_scene::FrameRenderer& frameRenderer,
    FrameRenderState& frameRenderState)
{
    frameRenderState.menuView = pauseMenu.buildView(controls);
    frameRenderState.spatialHud = {};
    frameRenderState.focusMarker = {};
    frameRenderState.focusHint = {};
    frameRenderState.targetHud = {};
    const render_scene::SceneView sceneView =
        render_scene::buildSceneView(cam, pauseMenu.cameraSettings(), appState.framebufferSize);
    const ui::InterfaceSettings& interfaceSettings = pauseMenu.interfaceSettings();
    const bool staticScene = runtime.simulation.simFrozen || pauseMenu.isOpen();

    if (simulation.hasBodies()) {
        const bool canReuseStaticSnapshot =
            staticScene &&
            frameRenderState.cachedStaticScene &&
            frameRenderState.cachedStaticSceneRevision == runtime.simulation.sceneRevision &&
            frameRenderState.sceneSnapshot.bodies.size() == simulation.bodies().size();
        if (!canReuseStaticSnapshot) {
            buildSceneSnapshot(
                simulation,
                runtime,
                runtime.simulation.fixedStep.alpha,
                frameRenderState.sceneSnapshot);
            frameRenderState.sceneSnapshot.instanceRevision = ++frameRenderState.sceneSnapshotRevisionCounter;
        }
        frameRenderState.cachedStaticScene = staticScene;
        frameRenderState.cachedStaticSceneRevision = runtime.simulation.sceneRevision;
        frameRenderState.sceneSnapshot.pathTrails = runtime.pathHistory;
        frameRenderState.sceneSnapshot.pathTrailsRevision = runtime.pathHistoryRevision;
    } else {
        frameRenderState.sceneSnapshot.bodies.clear();
        frameRenderState.sceneSnapshot.models.clear();
        frameRenderState.sceneSnapshot.pathTrails = runtime.pathHistory;
        frameRenderState.sceneSnapshot.instanceRevision = ++frameRenderState.sceneSnapshotRevisionCounter;
        frameRenderState.sceneSnapshot.pathTrailsRevision = runtime.pathHistoryRevision;
        frameRenderState.cachedStaticScene = staticScene;
        frameRenderState.cachedStaticSceneRevision = runtime.simulation.sceneRevision;
    }
    const int focusedIndex =
        findFocusedBodyIndex(frameRenderState.sceneSnapshot, runtime.focus.camera);
    const bool needsHoverTargetIndex = focusedIndex < 0 &&
        (interfaceSettings.objectInfo || interfaceSettings.showMinimap) &&
        !frameRenderState.sceneSnapshot.bodies.empty();
    const int targetIndex = focusedIndex >= 0
        ? focusedIndex
        : (needsHoverTargetIndex
            ? findTargetBodyIndex(frameRenderState.sceneSnapshot, cam, sceneView)
            : -1);
    buildFocusMarker(
        frameRenderState.sceneSnapshot,
        sceneView,
        appState.framebufferSize,
        focusedIndex,
        frameRenderState.focusMarker);
    buildTargetHud(
        frameRenderState.sceneSnapshot,
        interfaceSettings,
        sceneView,
        appState.framebufferSize,
        targetIndex,
        frameRenderState.targetHud);
    buildSpatialHud(
        frameRenderState.sceneSnapshot,
        interfaceSettings,
        cam,
        sceneView,
        targetIndex,
        frameRenderState.spatialHudScratch,
        frameRenderState.spatialHud);
    if (runtime.focus.camera.active) {
        frameRenderState.focusHint.visible = true;
        frameRenderState.focusHint.label =
            input::keyNameForCode(controls.focusTarget) + std::string{" OR MOVE TO EXIT FOCUS"};
        frameRenderState.focusHint.actionHint = "MOUSE WHEEL TO ZOOM";
    }

    const render_scene::OverlayPassInput overlayInput{
        &frameRenderState.menuView,
        &frameRenderState.spatialHud,
        &frameRenderState.focusMarker,
        &frameRenderState.focusHint,
        &frameRenderState.targetHud,
        pauseMenu.uiScale(),
        &interfaceSettings,
    };
    const render_scene::FrameInput frameInput{
        appState.framebufferSize,
        runtime.simulation.simFrozen,
        runtime.simulation.simSpeed,
        runtime.simulation.elapsedTime,
        runtime.fps.displayed,
        interfaceSettings.drawPath,
        interfaceSettings.pathColorIndex,
        selectedBackdropPreset(interfaceSettings),
        &sceneView,
        &frameRenderState.sceneSnapshot,
        &overlayInput,
    };
    frameRenderer.render(frameInput);

    glfwSwapBuffers(window);
}

} // namespace app_loop

#ifndef PHYSICS3D_RENDER_FRAMERENDERER_H
#define PHYSICS3D_RENDER_FRAMERENDERER_H

#include "render/SceneLighting.h"
#include "render/SceneRenderer.h"
#include "ui/OverlayRenderer.h"
#include "ui/PauseMenu.h"

#include <array>
#include <vector>

namespace render_scene {

struct OverlayPassInput {
    const ui::MenuView* menu = nullptr;
    const OverlayRenderer::SpatialHud* spatialHud = nullptr;
    const OverlayRenderer::TargetHud* targetHud = nullptr;
    float uiScale = 1.0f;
    const ui::InterfaceSettings* interfaceSettings = nullptr;
};

struct FrameInput {
    FramebufferSize framebufferSize{};
    bool simFrozen = false;
    double simSpeed = 1.0;
    double simElapsed = 0.0;
    double fps = 0.0;
    bool showWorldPaths = false;
    int pathColorIndex = 0;
    const SceneView* sceneView = nullptr;
    const SceneSnapshot* sceneSnapshot = nullptr;
    const OverlayPassInput* overlay = nullptr;
};

class FrameRenderer {
public:
    FrameRenderer() = default;
    FrameRenderer(const FrameRenderer&) = delete;
    FrameRenderer& operator=(const FrameRenderer&) = delete;
    ~FrameRenderer();

    bool init();
    void shutdown();
    void render(const FrameInput& input);

private:
    struct SceneResources {
        GLuint program = 0;
        GLint uView = -1;
        GLint uProj = -1;
        GLint uCameraPos = -1;
        GLint uLightDir = -1;
        GLint uLightColor = -1;
        GLint uSkyZenithColor = -1;
        GLint uSkyHorizonColor = -1;
        GLint uGroundColor = -1;
        GLint uLightViewProj = -1;
        GLint uShadowMap = -1;
        GLint uShadowTexelSize = -1;
        GLint uLocalLightCount = -1;
        std::array<GLint, kMaxLocalLights> uLocalLightPosRange{};
        std::array<GLint, kMaxLocalLights> uLocalLightColorIntensity{};
        GLuint sphereVao = 0;
        GLuint sphereVbo = 0;
        GLuint sphereEbo = 0;
        GLuint instanceVbo = 0;
        GLuint pathProgram = 0;
        GLint pathUViewProj = -1;
        GLint pathUColor = -1;
        GLuint pathVao = 0;
        GLuint pathVbo = 0;
        GLsizei sphereIndexCount = 0;

        void release()
        {
            if (pathVbo) glDeleteBuffers(1, &pathVbo);
            if (pathVao) glDeleteVertexArrays(1, &pathVao);
            if (pathProgram) glDeleteProgram(pathProgram);
            if (instanceVbo) glDeleteBuffers(1, &instanceVbo);
            if (sphereEbo) glDeleteBuffers(1, &sphereEbo);
            if (sphereVbo) glDeleteBuffers(1, &sphereVbo);
            if (sphereVao) glDeleteVertexArrays(1, &sphereVao);
            if (program) glDeleteProgram(program);
            *this = {};
        }
    };

    struct SkyResources {
        GLuint program = 0;
        GLuint fullscreenVao = 0;
        GLint uInvViewProj = -1;
        GLint uCameraPos = -1;
        GLint uLightDir = -1;
        GLint uSkyZenithColor = -1;
        GLint uSkyHorizonColor = -1;
        GLint uGroundColor = -1;
        GLint uSunGlowColor = -1;
        GLint uSkyAccentColor = -1;
        GLint uBackdropPreset = -1;
        GLint uStarIntensity = -1;
        GLint uNebulaIntensity = -1;
        GLint uStateTintColor = -1;
        GLint uStateTintStrength = -1;

        void release()
        {
            if (fullscreenVao) glDeleteVertexArrays(1, &fullscreenVao);
            if (program) glDeleteProgram(program);
            *this = {};
        }
    };

    struct SkyCacheResources {
        GLuint framebuffer = 0;
        GLuint colorTexture = 0;
        FramebufferSize framebufferSize{-1, -1};

        void release()
        {
            if (colorTexture) glDeleteTextures(1, &colorTexture);
            if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
            *this = {};
        }
    };

    struct ShadowResources {
        GLuint framebuffer = 0;
        GLuint depthTexture = 0;
        GLuint program = 0;
        GLint uLightViewProj = -1;
        int mapResolution = 2048;

        void release()
        {
            if (depthTexture) glDeleteTextures(1, &depthTexture);
            if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
            if (program) glDeleteProgram(program);
            *this = {};
        }
    };

    struct PostProcessResources {
        GLuint hdrFramebuffer = 0;
        GLuint hdrColorTexture = 0;
        GLuint hdrDepthTexture = 0;
        GLuint fullscreenVao = 0;
        GLuint program = 0;
        GLint uSceneColor = -1;
        GLint uSceneDepth = -1;
        GLint uBloomTexture = -1;
        GLint uExposure = -1;
        GLint uGamma = -1;
        GLint uBloomStrength = -1;
        GLint uContrast = -1;
        GLint uSaturation = -1;
        GLint uCameraNearFar = -1;
        GLint uFogNearColor = -1;
        GLint uFogFarColor = -1;
        GLint uFogDistances = -1;
        FramebufferSize framebufferSize{-1, -1};

        void release()
        {
            if (hdrDepthTexture) glDeleteTextures(1, &hdrDepthTexture);
            if (hdrColorTexture) glDeleteTextures(1, &hdrColorTexture);
            if (hdrFramebuffer) glDeleteFramebuffers(1, &hdrFramebuffer);
            if (fullscreenVao) glDeleteVertexArrays(1, &fullscreenVao);
            if (program) glDeleteProgram(program);
            *this = {};
        }
    };

    struct BloomResources {
        GLuint extractProgram = 0;
        GLint extractUSceneColor = -1;
        GLint extractUThreshold = -1;
        GLuint blurProgram = 0;
        GLint blurUSourceTexture = -1;
        GLint blurUTexelSize = -1;
        GLint blurUDirection = -1;
        GLuint framebuffers[2]{0, 0};
        GLuint textures[2]{0, 0};
        FramebufferSize framebufferSize{-1, -1};

        void release()
        {
            if (framebuffers[0]) glDeleteFramebuffers(2, framebuffers);
            if (textures[0]) glDeleteTextures(2, textures);
            if (blurProgram) glDeleteProgram(blurProgram);
            if (extractProgram) glDeleteProgram(extractProgram);
            *this = {};
        }
    };

    struct FrameScratch {
        InstanceBufferState instanceBufferState{};
        PathBufferState pathBufferState{};
        std::vector<SceneInstanceData> instanceScratch{};
        std::vector<glm::vec3> pathPointsScratch{};
        std::vector<GLint> pathDrawStarts{};
        std::vector<GLsizei> pathDrawCounts{};
    };

    struct LightingCacheKey {
        std::uint64_t instanceRevision = 0;
        int shadowMapResolution = 0;
        glm::vec3 cameraPosition{0.0f, 0.0f, 0.0f};
        glm::vec3 forward{0.0f, 0.0f, -1.0f};
    };

    struct SkyCacheKey {
        FramebufferSize framebufferSize{-1, -1};
        glm::vec3 forward{0.0f, 0.0f, -1.0f};
        glm::vec2 projScale{1.0f, 1.0f};
        glm::vec3 lightDir{0.0f, 1.0f, 0.0f};
        glm::vec3 skyZenithColor{0.0f, 0.0f, 0.0f};
        glm::vec3 skyHorizonColor{0.0f, 0.0f, 0.0f};
        glm::vec3 groundColor{0.0f, 0.0f, 0.0f};
        glm::vec3 sunGlowColor{0.0f, 0.0f, 0.0f};
        glm::vec3 skyAccentColor{0.0f, 0.0f, 0.0f};
        BackdropPreset backdropPreset = BackdropPreset::Sunny;
        float starIntensity = 0.0f;
        float nebulaIntensity = 0.0f;
        glm::vec3 stateTintColor{0.0f, 0.0f, 0.0f};
        float stateTintStrength = 0.0f;
    };

    bool initSceneResources_();
    bool initSkyResources_();
    bool initShadowResources_();
    bool initPostProcessResources_();
    bool initBloomResources_();
    bool resizeHdrTarget_(const FramebufferSize& framebufferSize);
    bool resizeSkyCacheTarget_(const FramebufferSize& framebufferSize);
    bool resizeBloomTargets_(const FramebufferSize& framebufferSize);
    [[nodiscard]] bool skyCacheMatches_(const SkyCacheKey& key) const;
    void applyViewport_(const FramebufferSize& framebufferSize);
    void prepareSceneInstances_(const SceneSnapshot& snapshot);
    [[nodiscard]] static glm::vec3 sceneBaseColor_(bool simFrozen, bool menuVisible);
    void bindShadowTarget_() const;
    void renderShadowPass_(const SceneLighting& lighting) const;
    void bindSceneTarget_(const FramebufferSize& framebufferSize) const;
    static void clearSceneDepth_();
    void updateSkyCache_(
        const FramebufferSize& framebufferSize,
        const SceneView& sceneView,
        const SceneLighting& lighting,
        const glm::vec3& stateTintColor,
        float stateTintStrength);
    void copySkyCacheToSceneTarget_(const FramebufferSize& framebufferSize) const;
    void renderSkyPass_(
        const SceneView& sceneView,
        const SceneLighting& lighting,
        const glm::vec3& stateTintColor,
        float stateTintStrength) const;
    void renderScenePass_(const FrameInput& input, const SceneLighting& lighting);
    void renderWorldPathPass_(const FrameInput& input);
    void renderBloomPass_() const;
    void compositeSceneTarget_(
        const FramebufferSize& framebufferSize,
        const SceneView& sceneView,
        const SceneLighting& lighting) const;
    void renderOverlayPass_(const FrameInput& input) const;

    SceneResources resources_{};
    SkyResources sky_{};
    SkyCacheResources skyCache_{};
    ShadowResources shadow_{};
    PostProcessResources postProcess_{};
    BloomResources bloom_{};
    FrameScratch scratch_{};
    OverlayRenderer overlay_{};
    LightingCacheKey lightingCacheKey_{};
    SkyCacheKey skyCacheKey_{};
    SceneLighting cachedLighting_{};
    FramebufferSize appliedViewport_{-1, -1};
    bool lightingCacheValid_ = false;
    bool skyCacheValid_ = false;
    bool initialized_ = false;
};

} // namespace render_scene

#endif // PHYSICS3D_RENDER_FRAMERENDERER_H

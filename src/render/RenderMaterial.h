#ifndef PHYSICS3D_RENDER_RENDERMATERIAL_H
#define PHYSICS3D_RENDER_RENDERMATERIAL_H

#include <glm/vec3.hpp>

#include <cstdint>
#include <string_view>

namespace render_scene {

enum class RenderSurfacePattern : std::uint8_t {
    Smooth = 0,
    Rocky = 1,
    Banded = 2,
    Stellar = 3,
};

struct RenderMaterial {
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metalness = 0.0f;
    glm::vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float emissiveIntensity = 0.0f;
    float detailScale = 0.0f;
    float detailStrength = 0.0f;
    float surfaceSeed = 0.0f;
    RenderSurfacePattern surfacePattern = RenderSurfacePattern::Smooth;
};

struct RenderMaterialInputs {
    std::string_view materialName{};
    float radius = 1.0f;
    double invMass = 0.0;
    std::uint64_t stableId = 0;
};

[[nodiscard]] const RenderMaterial& defaultRenderMaterial();
[[nodiscard]] const RenderMaterial* findRenderMaterial(std::string_view name);
[[nodiscard]] const RenderMaterial& resolveRenderMaterial(std::string_view name);
[[nodiscard]] RenderMaterial resolvePresentationMaterial(const RenderMaterialInputs& inputs);

} // namespace render_scene

#endif // PHYSICS3D_RENDER_RENDERMATERIAL_H

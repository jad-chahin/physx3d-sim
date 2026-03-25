#include "render/RenderMaterial.h"

#include <glm/common.hpp>

#include <array>

namespace render_scene {
namespace {

struct NamedRenderMaterial {
    std::string_view name;
    RenderMaterial material;
};

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] float stableUnitFloat(const std::uint64_t value)
{
    std::uint64_t x = value + 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30u)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27u)) * 0x94D049BB133111EBull;
    x ^= (x >> 31u);
    return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] glm::vec3 varyColor(
    const glm::vec3& a,
    const glm::vec3& b,
    const std::uint64_t stableId)
{
    return glm::mix(a, b, stableUnitFloat(stableId));
}

constexpr std::array<NamedRenderMaterial, 6> kBuiltinRenderMaterials{{
    {"DEFAULT", {{0.82f, 0.74f, 0.64f}, 0.62f, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, 3.2f, 0.18f, 0.0f, RenderSurfacePattern::Rocky}},
    {"STEEL", {{0.74f, 0.77f, 0.82f}, 0.24f, 1.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0.0f, RenderSurfacePattern::Smooth}},
    {"RUBBER", {{0.08f, 0.09f, 0.10f}, 0.92f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0.0f, RenderSurfacePattern::Smooth}},
    {"STONE", {{0.58f, 0.60f, 0.63f}, 0.88f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 3.6f, 0.14f, 0.0f, RenderSurfacePattern::Rocky}},
    {"WOOD", {{0.50f, 0.34f, 0.18f}, 0.72f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 1.8f, 0.08f, 0.0f, RenderSurfacePattern::Banded}},
    {"PLASTIC", {{0.14f, 0.36f, 0.78f}, 0.34f, 0.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0.0f, RenderSurfacePattern::Smooth}},
}};

[[nodiscard]] float densityProxy(const float radius, const double invMass)
{
    if (!(radius > 0.0f) || !(invMass > 0.0)) {
        return 0.0f;
    }
    const double mass = 1.0 / invMass;
    const double volume = (4.0 / 3.0) * static_cast<double>(kPi) * radius * radius * radius;
    if (!(volume > 0.0)) {
        return 0.0f;
    }
    return static_cast<float>(mass / volume);
}

[[nodiscard]] RenderMaterial makeStarMaterial(const RenderMaterialInputs& inputs)
{
    RenderMaterial material{};
    material.albedo = varyColor(glm::vec3(1.00f, 0.72f, 0.34f), glm::vec3(1.00f, 0.88f, 0.62f), inputs.stableId);
    material.roughness = 0.42f;
    material.metalness = 0.0f;
    material.emissiveColor = glm::mix(material.albedo, glm::vec3(1.0f, 0.92f, 0.82f), 0.45f);
    material.emissiveIntensity = 1.45f;
    material.detailScale = 4.8f;
    material.detailStrength = 0.26f;
    material.surfaceSeed = stableUnitFloat(inputs.stableId ^ 0xA51C52D7ull);
    material.surfacePattern = RenderSurfacePattern::Stellar;
    return material;
}

[[nodiscard]] RenderMaterial makeGasMaterial(const RenderMaterialInputs& inputs)
{
    RenderMaterial material{};
    material.albedo = varyColor(glm::vec3(0.78f, 0.58f, 0.34f), glm::vec3(0.44f, 0.60f, 0.84f), inputs.stableId ^ 0xBADC0FFEEull);
    material.roughness = 0.84f;
    material.metalness = 0.0f;
    material.detailScale = 2.2f + 1.2f * stableUnitFloat(inputs.stableId ^ 0x2209ull);
    material.detailStrength = 0.24f;
    material.surfaceSeed = stableUnitFloat(inputs.stableId ^ 0x33F1ull);
    material.surfacePattern = RenderSurfacePattern::Banded;
    return material;
}

[[nodiscard]] RenderMaterial makeMoonMaterial(const RenderMaterialInputs& inputs)
{
    RenderMaterial material{};
    material.albedo = varyColor(glm::vec3(0.56f, 0.55f, 0.53f), glm::vec3(0.72f, 0.71f, 0.69f), inputs.stableId ^ 0xC0DEull);
    material.roughness = 0.92f;
    material.metalness = 0.0f;
    material.detailScale = 4.4f;
    material.detailStrength = 0.16f;
    material.surfaceSeed = stableUnitFloat(inputs.stableId ^ 0x7788ull);
    material.surfacePattern = RenderSurfacePattern::Rocky;
    return material;
}

[[nodiscard]] RenderMaterial makeRockyPlanetMaterial(const RenderMaterialInputs& inputs, const float density)
{
    RenderMaterial material{};
    const glm::vec3 cool = varyColor(glm::vec3(0.30f, 0.42f, 0.58f), glm::vec3(0.58f, 0.52f, 0.40f), inputs.stableId ^ 0x4455ull);
    const glm::vec3 warm = varyColor(glm::vec3(0.48f, 0.34f, 0.22f), glm::vec3(0.72f, 0.68f, 0.54f), inputs.stableId ^ 0x9988ull);
    const float densityMix = glm::clamp((density - 2.3e8f) / 1.6e8f, 0.0f, 1.0f);
    material.albedo = glm::mix(cool, warm, densityMix);
    material.roughness = glm::mix(0.82f, 0.66f, densityMix);
    material.metalness = 0.02f;
    material.detailScale = 3.4f + stableUnitFloat(inputs.stableId ^ 0x1337ull);
    material.detailStrength = 0.18f;
    material.surfaceSeed = stableUnitFloat(inputs.stableId ^ 0xDEADull);
    material.surfacePattern = RenderSurfacePattern::Rocky;
    return material;
}

} // namespace

const RenderMaterial& defaultRenderMaterial()
{
    return kBuiltinRenderMaterials.front().material;
}

const RenderMaterial* findRenderMaterial(const std::string_view name)
{
    for (const auto& entry : kBuiltinRenderMaterials) {
        if (entry.name == name) {
            return &entry.material;
        }
    }
    return nullptr;
}

const RenderMaterial& resolveRenderMaterial(const std::string_view name)
{
    if (const RenderMaterial* material = findRenderMaterial(name); material != nullptr) {
        return *material;
    }
    return defaultRenderMaterial();
}

RenderMaterial resolvePresentationMaterial(const RenderMaterialInputs& inputs)
{
    const RenderMaterial base = resolveRenderMaterial(inputs.materialName);
    if (inputs.materialName != "DEFAULT") {
        RenderMaterial resolved = base;
        resolved.surfaceSeed = stableUnitFloat(inputs.stableId ^ 0x5EEDull);
        return resolved;
    }

    const float density = densityProxy(inputs.radius, inputs.invMass);
    if (inputs.radius >= 3.5f && density >= 5.0e8f) {
        return makeStarMaterial(inputs);
    }
    if (inputs.radius >= 1.8f && density > 0.0f && density < 1.5e8f) {
        return makeGasMaterial(inputs);
    }
    if (inputs.radius <= 0.6f) {
        return makeMoonMaterial(inputs);
    }
    return makeRockyPlanetMaterial(inputs, density);
}

} // namespace render_scene

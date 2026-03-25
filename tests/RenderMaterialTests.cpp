#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "render/RenderMaterial.h"

namespace {

void require(const bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] bool nearlyEqual(const float lhs, const float rhs, const float epsilon = 1e-4f)
{
    return std::abs(lhs - rhs) <= epsilon;
}

[[nodiscard]] double sphereInvMass(const double radius, const double density)
{
    constexpr double kPi = 3.14159265358979323846;
    const double volume = (4.0 / 3.0) * kPi * radius * radius * radius;
    return 1.0 / (volume * density);
}

void testResolveRenderMaterialFallsBackToDefault()
{
    const auto& fallback = render_scene::defaultRenderMaterial();
    const auto& resolved = render_scene::resolveRenderMaterial("UNKNOWN_MATERIAL");

    require(nearlyEqual(resolved.albedo.r, fallback.albedo.r), "unknown material should fall back to default albedo");
    require(nearlyEqual(resolved.roughness, fallback.roughness), "unknown material should fall back to default roughness");
    require(nearlyEqual(resolved.metalness, fallback.metalness), "unknown material should fall back to default metalness");
}

void testResolveRenderMaterialFindsBuiltinSteel()
{
    const auto& steel = render_scene::resolveRenderMaterial("STEEL");

    require(steel.metalness > 0.9f, "steel should resolve as a metallic material");
    require(steel.roughness < 0.35f, "steel should resolve with relatively low roughness");
}

void testResolvePresentationMaterialPromotesLargeDenseDefaultBodiesToStellar()
{
    const auto material = render_scene::resolvePresentationMaterial(render_scene::RenderMaterialInputs{
        "DEFAULT",
        4.6f,
        sphereInvMass(4.6, 1.25e9),
        1,
    });

    require(material.emissiveIntensity > 1.0f, "large dense default bodies should get a stellar emissive treatment");
    require(material.surfacePattern == render_scene::RenderSurfacePattern::Stellar,
        "large dense default bodies should get the stellar surface profile");
}

void testResolvePresentationMaterialPromotesLargeLightDefaultBodiesToBanded()
{
    const auto material = render_scene::resolvePresentationMaterial(render_scene::RenderMaterialInputs{
        "DEFAULT",
        2.15f,
        sphereInvMass(2.15, 8.5e7),
        4,
    });

    require(material.surfacePattern == render_scene::RenderSurfacePattern::Banded,
        "large light default bodies should get the banded gas-giant surface profile");
    require(material.detailStrength > 0.2f, "gas-giant presentation should keep visible band detail");
}

} // namespace

void appendRenderMaterialTests(std::vector<std::pair<std::string, std::function<void()>>>& tests)
{
    tests.emplace_back(
        "render_material_falls_back_to_default",
        testResolveRenderMaterialFallsBackToDefault);
    tests.emplace_back(
        "render_material_finds_builtin_steel",
        testResolveRenderMaterialFindsBuiltinSteel);
    tests.emplace_back(
        "render_material_promotes_large_dense_default_bodies_to_stellar",
        testResolvePresentationMaterialPromotesLargeDenseDefaultBodiesToStellar);
    tests.emplace_back(
        "render_material_promotes_large_light_default_bodies_to_banded",
        testResolvePresentationMaterialPromotesLargeLightDefaultBodiesToBanded);
}

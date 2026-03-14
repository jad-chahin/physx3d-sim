#include "sim/Material.h"

#include <algorithm>

#include "sim/Body.h"

namespace sim {
    namespace {
        const std::vector<Material> kBuiltinMaterials{
            Material{
                .name = "DEFAULT",
                .restitution = 0.5,
                .staticFriction = 0.6,
                .dynamicFriction = 0.4,
            },
        };
    } // namespace

    const Material& defaultMaterial() {
        return kBuiltinMaterials.front();
    }

    const std::vector<Material>& builtinMaterials() {
        return kBuiltinMaterials;
    }

    const Material* findBuiltinMaterial(const std::string_view name) {
        for (const auto& material : kBuiltinMaterials) {
            if (material.name == name) {
                return &material;
            }
        }
        return nullptr;
    }

    const Material& resolveMaterial(const std::string_view name) {
        if (const Material* material = findBuiltinMaterial(name); material != nullptr) {
            return *material;
        }
        return defaultMaterial();
    }

    void applyMaterial(Body& body, const Material& material) {
        body.materialName = material.name;
        body.restitution = std::clamp(material.restitution, 0.0, 1.0);
        body.staticFriction = std::max(0.0, material.staticFriction);
        body.dynamicFriction = std::max(0.0, std::min(material.dynamicFriction, body.staticFriction));
    }

    void assignMaterial(Body& body, const std::string_view name) {
        applyMaterial(body, resolveMaterial(name));
    }
} // namespace sim

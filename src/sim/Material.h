#ifndef PHYSICS3D_MATERIAL_H
#define PHYSICS3D_MATERIAL_H

#include <string>
#include <string_view>
#include <vector>

namespace sim {
    class Body;

    inline constexpr std::string_view kDefaultMaterialName = "DEFAULT";

    struct Material {
        std::string name;
        double restitution = 0.5;
        double staticFriction = 0.6;
        double dynamicFriction = 0.4;
    };

    [[nodiscard]] const Material& defaultMaterial();
    [[nodiscard]] const std::vector<Material>& builtinMaterials();
    [[nodiscard]] const Material* findBuiltinMaterial(std::string_view name);
    [[nodiscard]] const Material& resolveMaterial(std::string_view name);

    void applyMaterial(Body& body, const Material& material);
    void assignMaterial(Body& body, std::string_view name);
} // namespace sim

#endif // PHYSICS3D_MATERIAL_H

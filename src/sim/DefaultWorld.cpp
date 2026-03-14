#include "sim/DefaultWorld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>
#include "sim/Body.h"
#include "sim/Material.h"
#include "sim/Vec3.h"


namespace sim {
    namespace {
        constexpr double kCentralInvMass = 1e-12;
        constexpr double kCentralMass = 1.0 / kCentralInvMass;
        constexpr double kGravityStrength = World::Params::kDefaultG;
        constexpr double kOrbiterInvMass = 1.0;

        struct OrbiterSpec {
            Vec3 position;
            double radius = 1.0;
            Vec3 angularVelocity{};
        };

        std::size_t pushBall(
            std::vector<Body>& bodies,
            const Vec3& position,
            const Vec3& velocity,
            const double invMass,
            const double radius)
        {
            Body b{};
            b.position = position;
            b.prevPosition = position;
            b.velocity = velocity;
            b.invMass = std::max(0.0, invMass);
            b.radius = std::max(0.05, radius);
            assignMaterial(b, kDefaultMaterialName);
            bodies.push_back(b);
            return bodies.size() - 1;
        }

        [[nodiscard]] Vec3 circularOrbitVelocity(const Vec3& position) {
            const double radius = std::sqrt(position.x * position.x + position.y * position.y);
            if (radius <= 0.0) {
                return Vec3{};
            }

            const double speed = std::sqrt((kGravityStrength * kCentralMass) / radius);
            const Vec3 tangent(-position.y / radius, position.x / radius, 0.0);
            return tangent * speed;
        }

        std::size_t pushOrbiter(std::vector<Body>& bodies, const OrbiterSpec& spec) {
            const std::size_t bodyIndex = pushBall(
                bodies,
                spec.position,
                circularOrbitVelocity(spec.position),
                kOrbiterInvMass,
                spec.radius);
            bodies[bodyIndex].angularVelocity = spec.angularVelocity;
            return bodyIndex;
        }

        void applyDefaultWorldParams(World& world) {
            world.params().enableGravity = true;
            world.params().enableCollisions = true;
            world.params().collisionIterations = 2;
            world.params().G = kGravityStrength;
            world.params().restitution = World::Params::kDefaultRestitution;
        }
    }

    World makeDefaultWorld() {
        std::vector<Body> bodies;
        bodies.reserve(4);

        pushBall(
            bodies,
            Vec3(0.0, 0.0, 0.0),
            Vec3(0.0, 0.0, 0.0),
            kCentralInvMass,
            3.2);

        constexpr std::array<OrbiterSpec, 3> kOrbiters{{
            {.position = Vec3(0.0, 7.0, 0.0), .radius = 0.85},
            {.position = Vec3(-9.5, 0.0, 0.0), .radius = 0.75},
            {.position = Vec3(0.0, -13.0, 0.0), .radius = 0.60, .angularVelocity = Vec3(0.0, 2.0, 0.0)},
        }};
        for (const auto& orbiter : kOrbiters) {
            pushOrbiter(bodies, orbiter);
        }

        World world(std::move(bodies));
        applyDefaultWorldParams(world);
        return world;
    }
}

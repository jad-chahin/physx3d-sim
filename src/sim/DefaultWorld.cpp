#include "sim/DefaultWorld.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>
#include "sim/Body.h"
#include "sim/Material.h"
#include "sim/Vec3.h"


namespace sim {
    namespace {
        constexpr double kGravityStrength = World::Params::kDefaultG;
        constexpr double kMinRadius = 0.05;
        constexpr double kPi = 3.14159265358979323846;

        struct BodySpec {
            Vec3 position;
            double radius = 1.0;
            double density = 1.0;
            Vec3 velocity{};
            Vec3 angularVelocity{};
        };

        [[nodiscard]] double sphereMass(const double radius, const double density) {
            if (!std::isfinite(radius) || radius <= 0.0 || !std::isfinite(density) || density <= 0.0) {
                return 0.0;
            }
            return (4.0 / 3.0) * kPi * radius * radius * radius * density;
        }

        [[nodiscard]] Vec3 tangentialDirection(const Vec3& radial) {
            const double r2 = radial.x * radial.x + radial.y * radial.y;
            if (!(r2 > 0.0)) {
                return Vec3{};
            }
            const double invR = 1.0 / std::sqrt(r2);
            return Vec3(-radial.y * invR, radial.x * invR, 0.0);
        }

        [[nodiscard]] Vec3 circularOrbitVelocity(
            const Vec3& orbitCenter,
            const Vec3& position,
            const double primaryMass,
            const double secondaryMass = 0.0)
        {
            const Vec3 radial = position - orbitCenter;
            const double orbitalRadius = std::sqrt(radial.x * radial.x + radial.y * radial.y + radial.z * radial.z);
            if (!(orbitalRadius > 0.0) || !(primaryMass > 0.0)) {
                return Vec3{};
            }

            const double speed = std::sqrt(kGravityStrength * (primaryMass + std::max(0.0, secondaryMass)) / orbitalRadius);
            return tangentialDirection(radial) * speed;
        }

        std::size_t pushBody(
            std::vector<Body>& bodies,
            const BodySpec& spec)
        {
            Body b{};
            b.position = spec.position;
            b.prevPosition = spec.position;
            b.velocity = spec.velocity;
            b.angularVelocity = spec.angularVelocity;
            b.radius = std::max(kMinRadius, spec.radius);

            const double mass = sphereMass(b.radius, spec.density);
            b.invMass = mass > 0.0 ? (1.0 / mass) : 0.0;
            assignMaterial(b, kDefaultMaterialName);
            bodies.push_back(b);
            return bodies.size() - 1;
        }

        void recenterSystem(std::vector<Body>& bodies) {
            double totalMass = 0.0;
            Vec3 centerOfMass{};
            Vec3 totalMomentum{};

            for (const Body& body : bodies) {
                if (!(body.invMass > 0.0) || !std::isfinite(body.invMass)) {
                    continue;
                }
                const double mass = 1.0 / body.invMass;
                totalMass += mass;
                centerOfMass += body.position * mass;
                totalMomentum += body.velocity * mass;
            }

            if (!(totalMass > 0.0)) {
                return;
            }

            centerOfMass /= totalMass;
            const Vec3 barycentricVelocity = totalMomentum / totalMass;

            for (Body& body : bodies) {
                body.position -= centerOfMass;
                body.prevPosition = body.position;
                body.velocity -= barycentricVelocity;
            }
        }

        void applyDefaultWorldParams(World& world) {
            world.params().enableGravity = true;
            world.params().enableCollisions = true;
            world.params().velocityIterations = 2;
            world.params().positionIterations = 2;
            world.params().G = kGravityStrength;
            world.params().restitution = World::Params::kDefaultRestitution;
        }
    }

    World makeDefaultWorld() {
        std::vector<Body> bodies;
        bodies.reserve(5);

        BodySpec star{
            .position = Vec3(0.0, 0.0, 0.0),
            .radius = 4.6,
            .density = 1.25e9,
            .angularVelocity = Vec3(0.0, 0.08, 0.0),
        };
        const double starMass = sphereMass(star.radius, star.density);

        BodySpec innerPlanet{
            .position = Vec3(0.0, 15.5, 0.0),
            .radius = 0.92,
            .density = 3.4e8,
            .angularVelocity = Vec3(0.0, 0.4, 0.0),
        };
        const double innerPlanetMass = sphereMass(innerPlanet.radius, innerPlanet.density);
        innerPlanet.velocity = circularOrbitVelocity(star.position, innerPlanet.position, starMass, innerPlanetMass);

        BodySpec outerPlanet{
            .position = Vec3(-24.0, 0.0, 0.0),
            .radius = 1.15,
            .density = 2.6e8,
            .angularVelocity = Vec3(0.0, 0.22, 0.0),
        };
        const double outerPlanetMass = sphereMass(outerPlanet.radius, outerPlanet.density);
        outerPlanet.velocity = circularOrbitVelocity(star.position, outerPlanet.position, starMass, outerPlanetMass);

        BodySpec gasGiant{
            .position = Vec3(0.0, -39.0, 0.0),
            .radius = 2.15,
            .density = 8.5e7,
            .angularVelocity = Vec3(0.0, 0.12, 0.0),
        };
        const double gasGiantMass = sphereMass(gasGiant.radius, gasGiant.density);
        gasGiant.velocity = circularOrbitVelocity(star.position, gasGiant.position, starMass, gasGiantMass);

        BodySpec moon{
            .position = gasGiant.position + Vec3(4.8, 0.0, 0.0),
            .radius = 0.46,
            .density = 3.0e8,
            .angularVelocity = Vec3(0.0, 0.6, 0.0),
        };
        const double moonMass = sphereMass(moon.radius, moon.density);
        moon.velocity =
            gasGiant.velocity + circularOrbitVelocity(gasGiant.position, moon.position, gasGiantMass, moonMass);

        pushBody(bodies, star);
        pushBody(bodies, innerPlanet);
        pushBody(bodies, outerPlanet);
        pushBody(bodies, gasGiant);
        pushBody(bodies, moon);
        recenterSystem(bodies);

        World world(std::move(bodies));
        applyDefaultWorldParams(world);
        return world;
    }
}

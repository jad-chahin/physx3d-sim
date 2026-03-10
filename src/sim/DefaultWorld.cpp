#include "sim/DefaultWorld.h"

#include <algorithm>
#include <utility>
#include <vector>
#include "sim/Body.h"
#include "sim/Vec3.h"


namespace sim {
    namespace {
        std::size_t pushBall(
            std::vector<Body>& bodies,
            const Vec3& position,
            const Vec3& velocity,
            const double invMass,
            const double radius,
            const double restitution,
            const double staticFriction,
            const double dynamicFriction)
        {
            Body b{};
            b.position = position;
            b.prevPosition = position;
            b.velocity = velocity;
            b.invMass = std::max(0.0, invMass);
            b.radius = std::max(0.05, radius);
            b.restitution = std::clamp(restitution, 0.0, 1.0);
            b.staticFriction = std::max(0.0, staticFriction);
            b.dynamicFriction = std::max(0.0, std::min(dynamicFriction, b.staticFriction));
            bodies.push_back(b);
            return bodies.size() - 1;
        }
    }

    World makeDefaultWorld() {
        std::vector<Body> bodies;
        bodies.reserve(40);

        pushBall(
            bodies,
            Vec3(0.0, 0.0, 0.0),
            Vec3(0.0, 0.0, 0.0),
            1e-12, 3.2, 0.25, 0.85, 0.65);

        pushBall(
            bodies,
            Vec3(0.0, 4.05, 0.0),
            Vec3(0.0, 0.0, 0.0),
            1.0, 0.85, 0.02, 1.20, 1.00);

        pushBall(
            bodies,
            Vec3(0.0, -4.05, 0.0),
            Vec3(1.2, 0.0, 0.0),
            1.0, 0.85, 0.15, 0.55, 0.35);

        const std::size_t orbiter = pushBall(
            bodies,
            Vec3(0.0, 10.0, 0.0),
            Vec3(2.6, 0.0, 0.0),
            1.0, 0.60, 0.30, 0.15, 0.08);
        bodies[orbiter].angularVelocity = Vec3(0.0, 2.0, 0.0);

        World world(std::move(bodies));
        world.params().jointIterations = 12;
        world.params().collisionIterations = 2;
        world.params().G = 6.6743e-11;
        return world;
    }
}

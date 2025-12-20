//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_WORLD_H
#define PHYSICS3D_WORLD_H

#include <complex>
#include <vector>
#include "Body.h"

namespace sim {

    class World {
    public:
        struct Params {
            double G;
            double restitution;
            double penetrationSlop;
            double positionCorrectionPercent;
            int collisionIterations;
            bool enableGravity;
            bool enableCollisions;

            Params()
            : G(6.6743e-11),
              restitution(0.5),
              penetrationSlop(1e-4),
              positionCorrectionPercent(0.8),
              collisionIterations(1),
              enableGravity(true),
              enableCollisions(true) {}
        };

        World() = default;

        explicit World(const Params &params);
        World(std::vector<Body> bodies, const Params &params = Params());


        // One tick
        void step(double dt);

        // Body management
        void addBody(const Body& b);
        void clear();

        // Accessors
        std::vector<Body>& bodies();
        [[nodiscard]] const std::vector<Body>& bodies() const;

        Params& params();
        [[nodiscard]] const Params& params() const;

    private:
        Params params_;
        std::vector<Body> bodies_{};

        // Per-step accumulators (net force per body for the current tick)
        std::vector<Vec3> forces_{};

    private:
        // Buffer management
        void ensureBuffers_();

        // Step sub-stages
        void resetForces_();
        void computeForces_(); // currently: gravity only
        void integrate_(double dt); // update velocity + position from forces_
        void collide_(); // detect + resolve collisions

        // Force helpers
        void applyGravityPair_(size_t i, size_t j);

        // Collision helpers (sphere-sphere)
        void solveCollisionPair_(size_t i, size_t j);
    };

} // sim

#endif // PHYSICS3D_WORLD_H

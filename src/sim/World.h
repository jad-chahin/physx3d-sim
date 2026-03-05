//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_WORLD_H
#define PHYSICS3D_WORLD_H

#include <cstddef>
#include <utility>
#include <vector>
#include "Body.h"


namespace sim {

    class World {
    public:
        struct Metrics {
            std::size_t broadphaseCandidatesDiscrete = 0;
            std::size_t broadphaseCandidatesSwept = 0;
            std::size_t pairsVisited = 0;
            std::size_t pairsImpulseApplied = 0;
            std::size_t ccdEvents = 0;
            std::size_t ccdZeroTimePairsResolved = 0;
        };

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

        // Constructors
        World() = default;
        explicit World(const Params &params);
        explicit World(std::vector<Body> bodies);
        World(std::vector<Body> bodies, const Params &params);

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
        [[nodiscard]] const Metrics& metrics() const;

    private:
        Params params_;
        Metrics metrics_{};
        std::vector<Body> bodies_{};

        // Net force per body for the current tick
        std::vector<Vec3> forces_{};

    private:
        // Buffer management
        void syncForces_();

        // Step sub-stages
        void resetForces_();
        void computeForces_(); // Currently gravity only
        void integrate_(double dt); // Semi-implicit Euler velocity update
        void advancePositions_(double dt);
        void moveBodiesWithCCD_(double dt);

        // Force helpers
        void applyGravityPair_(std::size_t i, std::size_t j);

        // Collision helpers
        void collide_(); // detect + resolve collisions
        void collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, int iterations);
        [[nodiscard]] bool hasAnyOverlapInPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs) const;
    };

} // sim

#endif // PHYSICS3D_WORLD_H

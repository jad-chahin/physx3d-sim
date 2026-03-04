//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_WORLD_H
#define PHYSICS3D_WORLD_H

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

    private:
        Params params_;
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
        [[nodiscard]] double epsilon(std::size_t i, std::size_t j) const;
        void applyGravityPair_(std::size_t i, std::size_t j);

        // Collision helpers
        [[nodiscard]] bool sweptCollisionTime_(std::size_t i, std::size_t j, double maxTime, double& outTime) const;
        [[nodiscard]] bool findEarliestCollision_(double maxTime, std::size_t& outI, std::size_t& outJ, double& outTime) const;
        void collide_(); // detect + resolve collisions
        [[nodiscard]] bool isColliding_(std::size_t i, std::size_t j) const;
        void solveCollisionPair_(std::size_t i, std::size_t j, bool assumeColliding = false);
    };

} // sim

#endif // PHYSICS3D_WORLD_H

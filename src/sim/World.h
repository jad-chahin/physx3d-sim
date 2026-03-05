//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_WORLD_H
#define PHYSICS3D_WORLD_H

#include <cstdint>
#include <cstddef>
#include <unordered_map>
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
            std::size_t warmStartedPairs = 0;
            std::size_t manifoldActivePairs = 0;
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
        using ContactKey = std::pair<std::uint64_t, std::uint64_t>;

        struct PairHash {
            std::size_t operator()(const ContactKey& p) const;
        };

        struct ContactManifold {
            double normalImpulse = 0.0;
            bool touched = false;
            std::size_t staleFrames = 0;
        };

        Params params_;
        Metrics metrics_{};
        std::vector<Body> bodies_{};
        std::unordered_map<ContactKey, ContactManifold, PairHash> contactCache_{};
        std::uint64_t nextBodyId_ = 1;

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
        void collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, int iterations);
        [[nodiscard]] bool hasAnyOverlapInPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs) const;
        [[nodiscard]] ContactKey contactKeyForPair_(std::size_t i, std::size_t j) const;
        void assignBodyId_(Body& b);
        void beginContactFrame_();
        void warmStartPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs);
        void endContactFrame_();
    };

} // sim

#endif // PHYSICS3D_WORLD_H

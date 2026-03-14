#ifndef PHYSICS3D_WORLD_H
#define PHYSICS3D_WORLD_H

#include <cstdint>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Body.h"
#include "Collision.h"

namespace sim {

    class World {
    public:
        struct Metrics {
            // Lightweight per-step counters surfaced through the optional HUD diagnostics.
            std::size_t broadphaseCandidatesDiscrete = 0;
            std::size_t broadphaseCandidatesSwept = 0;
            std::size_t pairsVisited = 0;
            std::size_t pairsImpulseApplied = 0;
            std::size_t ccdEvents = 0;
            std::size_t ccdZeroTimePairsResolved = 0;
            std::size_t warmStartedPairs = 0;
            std::size_t manifoldActivePairs = 0;
            std::size_t sanitizedBodies = 0;
            std::size_t sanitizedFields = 0;
        };

        struct Params {
            static constexpr double kDefaultG = 6.6743e-11;
            static constexpr double kDefaultRestitution = 0.5;
            static constexpr double kDefaultPenetrationSlop = 1e-4;
            static constexpr double kDefaultPositionCorrectionPercent = 0.8;
            static constexpr int kDefaultCollisionIterations = 1;

            double G = kDefaultG;
            double restitution = kDefaultRestitution; // Global upper bound for contact restitution [0..1]
            double penetrationSlop = kDefaultPenetrationSlop; // Advanced collision tuning
            double positionCorrectionPercent = kDefaultPositionCorrectionPercent; // Advanced collision tuning
            int collisionIterations = kDefaultCollisionIterations;
            bool enableGravity = true;
            bool enableCollisions = true;
        };

        World() = default;
        explicit World(const Params& params);
        explicit World(std::vector<Body> bodies);
        World(std::vector<Body> bodies, const Params& params);

        void step(double dt);

        void addBody(const Body& b);
        void clear();

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

        struct ActiveCollisionPair {
            std::size_t i = 0;
            std::size_t j = 0;
            ContactKey key{};
            collision::SolveParams params{};
            double accumulatedImpulse = 0.0;
        };

        Params params_;
        Metrics metrics_{};
        std::vector<Body> bodies_{};
        std::unordered_map<ContactKey, ContactManifold, PairHash> contactCache_{};
        std::uint64_t nextBodyId_ = 1;

        std::vector<Vec3> forces_{};
        void prepareForces_();
        void computeForces_();
        void integrate_(double dt);
        void advancePositions_(double dt);
        void moveBodiesWithCCD_(double dt);
        void sanitizeBodies_();
        void applyGravityPair_(std::size_t i, std::size_t j);

        void collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, int iterations);
        [[nodiscard]] ContactKey contactKeyForPair_(std::size_t i, std::size_t j) const;
        [[nodiscard]] collision::SolveParams solveParamsForPair_(std::size_t i, std::size_t j) const;
        void assignBodyId_(Body& b);
        void initBodies_();
        void beginContactFrame_();
        void warmStartPairs_(std::span<const ActiveCollisionPair> pairs);
        void endContactFrame_();
    };

} // sim

#endif // PHYSICS3D_WORLD_H

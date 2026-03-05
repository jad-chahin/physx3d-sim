//
// Created by jchah on 2025-12-19.
//

#include "World.h"
#include "Broadphase.h"
#include "Collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <unordered_set>

namespace sim {
    std::size_t World::PairHash::operator()(const ContactKey& p) const
    {
        std::size_t h = 0xcbf29ce484222325ULL;
        h ^= std::hash<std::uint64_t>{}(p.first) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<std::uint64_t>{}(p.second) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }

    World::World(const Params& params)
        : params_(params) {}

    World::World(std::vector<Body> bodies)
        : bodies_(std::move(bodies))
    {
        for (auto& b : bodies_) {
            assignBodyId_(b);
        }
    }

    World::World(std::vector<Body> bodies, const Params& params)
        : params_(params), bodies_(std::move(bodies))
    {
        for (auto& b : bodies_) {
            assignBodyId_(b);
        }
    }

    void World::step(const double dt)
    {
        metrics_ = {};
        beginContactFrame_();
        syncForces_();
        resetForces_();
        computeForces_();
        integrate_(dt);
        moveBodiesWithCCD_(dt);
        endContactFrame_();
        metrics_.manifoldActivePairs = contactCache_.size();
    }

    void World::addBody(const Body& b)
    {
        bodies_.push_back(b);
        assignBodyId_(bodies_.back());
    }

    void World::clear()
    {
        bodies_.clear();
        forces_.clear();
        contactCache_.clear();
    }

    std::vector<Body>& World::bodies()
    {
        return bodies_;
    }

    const std::vector<Body>& World::bodies() const
    {
        return bodies_;
    }

    World::Params& World::params()
    {
        return params_;
    }

    const World::Params& World::params() const
    {
        return params_;
    }

    const World::Metrics& World::metrics() const
    {
        return metrics_;
    }

    void World::syncForces_()
    {
        if (bodies_.size() != forces_.size()) {
            forces_.resize(bodies_.size());
        }
    }

    void World::resetForces_()
    {
        for (auto& force : forces_) {
            force = Vec3(0.0, 0.0, 0.0);
        }
    }

    void World::computeForces_()
    {
        if (!params_.enableGravity) {
            return;
        }

        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
                applyGravityPair_(i, j);
            }
        }
    }

    void World::applyGravityPair_(const std::size_t i, const std::size_t j)
    {
        const Body& A = bodies_[i];
        const Body& B = bodies_[j];

        if (A.invMass == 0.0 || B.invMass == 0.0) {
            return;
        }

        const Vec3 d = B.position - A.position;
        const double r2 = d.dot(d);
        const double eps = (A.radius + B.radius) * 1e-6;
        const double r2Soft = r2 + eps * eps;

        const double m1 = 1.0 / A.invMass;
        const double m2 = 1.0 / B.invMass;

        const double invR = 1.0 / std::sqrt(r2Soft);
        const double invR3 = invR * invR * invR;

        const Vec3 f12 = d * (params_.G * m1 * m2 * invR3);
        forces_[i] = forces_[i] + f12;
        forces_[j] = forces_[j] - f12;
    }

    void World::integrate_(const double dt)
    {
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            Body& b = bodies_[i];
            if (b.invMass == 0.0) {
                continue;
            }
            const Vec3 a = forces_[i] * b.invMass;
            b.velocity = b.velocity + a * dt;
        }
    }

    void World::advancePositions_(const double dt)
    {
        for (auto& b : bodies_) {
            b.position = b.position + b.velocity * dt;
        }
    }

    void World::moveBodiesWithCCD_(const double dt)
    {
        if (!params_.enableCollisions) {
            advancePositions_(dt);
            return;
        }

        const collision::SolveParams solveParams{
            params_.restitution,
            params_.penetrationSlop,
            params_.positionCorrectionPercent
        };

        double remaining = dt;
        const double machineEps = std::numeric_limits<double>::epsilon();
        const double timeTol = machineEps * std::max(1.0, std::abs(dt));
        std::unordered_set<ContactKey, World::PairHash> zeroTimeResolved;

        while (remaining > timeTol) {
            const auto pairs = broadphase::sweptPairs(bodies_, remaining);
            metrics_.broadphaseCandidatesSwept += pairs.size();

            bool found = false;
            std::size_t iHit = 0;
            std::size_t jHit = 0;
            double tHit = std::numeric_limits<double>::infinity();

            for (const auto& [i, j] : pairs) {
                const Body& A = bodies_[i];
                const Body& B = bodies_[j];
                if (A.invMass == 0.0 && B.invMass == 0.0) {
                    continue;
                }

                const ContactKey key = contactKeyForPair_(i, j);
                if (zeroTimeResolved.find(key) != zeroTimeResolved.end()) {
                    continue;
                }

                double t = 0.0;
                if (!collision::sweptCollisionTime(A, B, remaining, t)) {
                    continue;
                }

                if (t < tHit) {
                    tHit = t;
                    iHit = i;
                    jHit = j;
                    found = true;
                }
            }

            if (!found) {
                advancePositions_(remaining);
                remaining = 0.0;
                break;
            }

            const ContactKey hitKey = contactKeyForPair_(iHit, jHit);

            if (tHit > timeTol) {
                advancePositions_(tHit);
                remaining -= tHit;
                zeroTimeResolved.clear();
            }

            warmStartPairs_({{iHit, jHit}});
            const auto stats = collision::solveCollisionPair(bodies_[iHit], bodies_[jHit], solveParams, true);
            metrics_.pairsVisited += stats.visited ? 1 : 0;
            metrics_.pairsImpulseApplied += stats.impulseApplied ? 1 : 0;
            ++metrics_.ccdEvents;

            const ContactKey key = contactKeyForPair_(iHit, jHit);
            if (collision::isColliding(bodies_[iHit], bodies_[jHit])) {
                ContactManifold& manifold = contactCache_[key];
                manifold.touched = true;
                manifold.staleFrames = 0;
                if (stats.normalImpulse > 0.0) {
                    manifold.normalImpulse = stats.normalImpulse;
                } else {
                    manifold.normalImpulse *= 0.9;
                }
            }

            if (tHit <= timeTol) {
                zeroTimeResolved.insert(hitKey);
                ++metrics_.ccdZeroTimePairsResolved;
            }
        }

        const auto cleanupPairs = broadphase::discretePairs(bodies_);
        metrics_.broadphaseCandidatesDiscrete += cleanupPairs.size();
        if (hasAnyOverlapInPairs_(cleanupPairs)) {
            collidePairs_(cleanupPairs, params_.collisionIterations);
        }
    }

    void World::collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, const int iterations)
    {
        if (pairs.empty() || iterations <= 0) {
            return;
        }

        const collision::SolveParams solveParams{
            params_.restitution,
            params_.penetrationSlop,
            params_.positionCorrectionPercent
        };

        warmStartPairs_(pairs);

        std::unordered_map<ContactKey, double, World::PairHash> accumulatedImpulse;
        accumulatedImpulse.reserve(pairs.size() * 2);

        for (int it = 0; it < iterations; ++it) {
            for (const auto& [i, j] : pairs) {
                const auto stats = collision::solveCollisionPair(bodies_[i], bodies_[j], solveParams, false);
                metrics_.pairsVisited += stats.visited ? 1 : 0;
                metrics_.pairsImpulseApplied += stats.impulseApplied ? 1 : 0;

                if (!stats.hasNormal) {
                    continue;
                }

                const ContactKey key = contactKeyForPair_(i, j);
                if (collision::isColliding(bodies_[i], bodies_[j])) {
                    ContactManifold& manifold = contactCache_[key];
                    manifold.touched = true;
                    manifold.staleFrames = 0;
                    if (stats.normalImpulse > 0.0) {
                        accumulatedImpulse[key] += stats.normalImpulse;
                    }
                }
            }
        }

        for (const auto& [i, j] : pairs) {
            const ContactKey key = contactKeyForPair_(i, j);
            if (!collision::isColliding(bodies_[i], bodies_[j])) {
                continue;
            }

            ContactManifold& manifold = contactCache_[key];
            manifold.touched = true;
            manifold.staleFrames = 0;

            const auto it = accumulatedImpulse.find(key);
            if (it != accumulatedImpulse.end()) {
                manifold.normalImpulse = it->second;
            } else {
                manifold.normalImpulse *= 0.9; // mild decay if no new impulse this frame
            }
        }
    }

    bool World::hasAnyOverlapInPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs) const
    {
        for (const auto& [i, j] : pairs) {
            if (collision::isColliding(bodies_[i], bodies_[j])) {
                return true;
            }
        }
        return false;
    }

    World::ContactKey World::contactKeyForPair_(const std::size_t i, const std::size_t j) const
    {
        const std::uint64_t a = bodies_[i].id;
        const std::uint64_t b = bodies_[j].id;
        return (a < b) ? ContactKey{a, b} : ContactKey{b, a};
    }

    void World::assignBodyId_(Body& b)
    {
        b.id = nextBodyId_++;
    }

    void World::beginContactFrame_()
    {
        for (auto& [key, manifold] : contactCache_) {
            (void)key;
            manifold.touched = false;
        }
    }

    void World::warmStartPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs)
    {
        constexpr double kWarmStartFactor = 0.85;
        for (const auto& [i, j] : pairs) {
            const ContactKey key = contactKeyForPair_(i, j);
            auto it = contactCache_.find(key);
            if (it == contactCache_.end()) {
                continue;
            }

            ContactManifold& manifold = it->second;
            if (manifold.normalImpulse <= 0.0) {
                continue;
            }
            if (!collision::isColliding(bodies_[i], bodies_[j])) {
                continue;
            }

            Vec3 n{};
            if (!collision::contactNormal(bodies_[i], bodies_[j], n)) {
                continue;
            }

            // Warm-start only when currently approaching or resting, never separating.
            const double vN = (bodies_[j].velocity - bodies_[i].velocity).dot(n);
            if (vN > 0.0) {
                continue;
            }

            collision::applyNormalImpulse(
                bodies_[i], bodies_[j], n, manifold.normalImpulse * kWarmStartFactor);
            manifold.touched = true;
            manifold.staleFrames = 0;
            ++metrics_.warmStartedPairs;
        }
    }

    void World::endContactFrame_()
    {
        for (auto it = contactCache_.begin(); it != contactCache_.end(); ) {
            ContactManifold& manifold = it->second;
            if (manifold.touched) {
                manifold.staleFrames = 0;
                ++it;
                continue;
            }

            ++manifold.staleFrames;
            manifold.normalImpulse *= 0.75;
            if (manifold.staleFrames > 2 || manifold.normalImpulse < 1e-8) {
                it = contactCache_.erase(it);
            } else {
                ++it;
            }
        }
    }
} // namespace sim

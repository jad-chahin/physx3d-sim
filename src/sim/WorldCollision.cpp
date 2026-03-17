#include "World.h"

#include "Broadphase.h"
#include "Collision.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace sim {

    void World::moveBodiesWithCCD_(const double dt)
    {
        if (!params_.enableCollisions) {
            advancePositions_(dt);
            return;
        }

        double remaining = dt;
        constexpr double machineEps = std::numeric_limits<double>::epsilon();
        const double timeTol = machineEps * std::max(1.0, std::abs(dt));
        if (remaining <= timeTol) {
            return;
        }

        thread_local std::vector<broadphase::Pair> sweptPairs;
        thread_local std::vector<broadphase::Pair> overlapPairs;
        thread_local std::vector<std::pair<std::size_t, std::size_t>> zeroTimeOverlapPairs;
        const int maxCcdIterations = std::max(1, params_.maxCcdIterationsPerStep);
        const int maxRepeatedZeroToiPairs = std::max(1, params_.maxRepeatedZeroToiPairs);
        ContactKey lastZeroToiKey{};
        bool lastZeroToiKeyValid = false;
        int repeatedZeroToiPairs = 0;
        int ccdIterations = 0;

        while (remaining > timeTol) {
            ++ccdIterations;
            ++metrics_.ccdIterations;
            if (ccdIterations > maxCcdIterations) {
                advancePositions_(remaining);
                broadphase::discretePairs(bodies_, overlapPairs);
                metrics_.broadphaseCandidatesDiscrete += overlapPairs.size();
                zeroTimeOverlapPairs.clear();
                zeroTimeOverlapPairs.reserve(overlapPairs.size());
                for (const auto& [i, j] : overlapPairs) {
                    if (collision::isColliding(bodies_[i], bodies_[j])) {
                        zeroTimeOverlapPairs.emplace_back(i, j);
                    }
                }
                if (!zeroTimeOverlapPairs.empty()) {
                    collidePairs_(zeroTimeOverlapPairs, params_.velocityIterations, params_.positionIterations);
                    metrics_.ccdZeroTimePairsResolved += zeroTimeOverlapPairs.size();
                }
                ++metrics_.ccdBudgetExhaustions;
                break;
            }

            broadphase::sweptPairs(bodies_, remaining, sweptPairs);
            metrics_.broadphaseCandidatesSwept += sweptPairs.size();

            bool found = false;
            double tHit = std::numeric_limits<double>::infinity();
            std::size_t toiI = 0;
            std::size_t toiJ = 0;
            for (const auto& [i, j] : sweptPairs) {
                const Body& A = bodies_[i];
                const Body& B = bodies_[j];
                if (A.invMass == 0.0 && B.invMass == 0.0) {
                    continue;
                }

                double t = 0.0;
                if (!collision::sweptCollisionTime(A, B, remaining, t)) {
                    continue;
                }
                if (t < tHit) {
                    tHit = t;
                    toiI = i;
                    toiJ = j;
                    found = true;
                }
            }

            if (!found) {
                advancePositions_(remaining);
                break;
            }

            bool advancedToToi = false;
            if (tHit > timeTol) {
                const double consumeToToi = std::min(tHit, remaining);
                if (consumeToToi > timeTol) {
                    advancePositions_(consumeToToi);
                    remaining = std::max(0.0, remaining - consumeToToi);
                    advancedToToi = true;
                    lastZeroToiKeyValid = false;
                    repeatedZeroToiPairs = 0;
                }
            }

            const ContactKey toiKey = contactKeyForPair_(toiI, toiJ);
            if (!advancedToToi) {
                if (lastZeroToiKeyValid && toiKey == lastZeroToiKey) {
                    ++repeatedZeroToiPairs;
                } else {
                    lastZeroToiKey = toiKey;
                    lastZeroToiKeyValid = true;
                    repeatedZeroToiPairs = 1;
                }
            }

            const auto toiStats = collision::solveCollisionPair(
                bodies_[toiI], bodies_[toiJ], solveParamsForPair_(toiI, toiJ), true);
            metrics_.pairsVisited += toiStats.visited ? 1 : 0;
            metrics_.pairsImpulseApplied += toiStats.impulseApplied ? 1 : 0;
            if (toiStats.impulseApplied) {
                wakeBody_(bodies_[toiI]);
                wakeBody_(bodies_[toiJ]);
            }

            if (toiStats.hasNormal) {
                ContactManifold& manifold = contactCache_[contactKeyForPair_(toiI, toiJ)];
                manifold.touched = true;
                manifold.staleFrames = 0;
                manifold.normal = toiStats.normal;
                if (toiStats.normalImpulse > 0.0) {
                    manifold.normalImpulse = toiStats.normalImpulse;
                } else {
                    manifold.normalImpulse *= 0.9;
                }
                if (std::abs(toiStats.tangentImpulse) > 0.0) {
                    manifold.tangentImpulse = toiStats.tangentImpulse;
                } else {
                    manifold.tangentImpulse *= 0.8;
                }
                contactTouchedBodies_[toiI] = true;
                contactTouchedBodies_[toiJ] = true;
            }
            ++metrics_.ccdEvents;

            broadphase::discretePairs(bodies_, overlapPairs);
            metrics_.broadphaseCandidatesDiscrete += overlapPairs.size();
            zeroTimeOverlapPairs.clear();
            zeroTimeOverlapPairs.reserve(overlapPairs.size());
            for (const auto& [i, j] : overlapPairs) {
                const Body& A = bodies_[i];
                const Body& B = bodies_[j];
                if (A.invMass == 0.0 && B.invMass == 0.0) {
                    continue;
                }
                if (collision::isColliding(A, B)) {
                    zeroTimeOverlapPairs.emplace_back(i, j);
                }
            }

            if (!zeroTimeOverlapPairs.empty()) {
                collidePairs_(zeroTimeOverlapPairs, params_.velocityIterations, params_.positionIterations);
                ++metrics_.ccdEvents;
                if (params_.velocityIterations > 0 || params_.positionIterations > 0) {
                    metrics_.ccdZeroTimePairsResolved += zeroTimeOverlapPairs.size();
                }
            }

            if (!advancedToToi) {
                // Ensure forward progress for repeated zero-time TOI contacts.
                const double repeatedPairBoost =
                    repeatedZeroToiPairs >= maxRepeatedZeroToiPairs ? std::max(1.0, static_cast<double>(repeatedZeroToiPairs)) : 1.0;
                const double minConsume =
                    std::max(timeTol * 32.0, (std::max(0.0, dt) / 128.0) * repeatedPairBoost);
                const double consume = std::min(remaining, minConsume);
                if (consume > 0.0) {
                    advancePositions_(consume);
                    remaining = std::max(0.0, remaining - consume);
                } else {
                    break;
                }
            }
        }
    }

    void World::collidePairs_(
        const std::vector<std::pair<std::size_t, std::size_t>>& pairs,
        const int velocityIterations,
        const int positionIterations)
    {
        if (pairs.empty() || (velocityIterations <= 0 && positionIterations <= 0)) {
            return;
        }

        thread_local std::vector<ActiveCollisionPair> activePairs;
        activePairs.clear();
        activePairs.reserve(pairs.size());
        for (const auto& [i, j] : pairs) {
            activePairs.push_back(ActiveCollisionPair{
                .i = i,
                .j = j,
                .key = contactKeyForPair_(i, j),
                .params = solveParamsForPair_(i, j),
            });
        }
        if (activePairs.empty()) {
            return;
        }

        warmStartPairs_(activePairs);

        for (int it = 0; it < positionIterations; ++it) {
            for (auto& pair : activePairs) {
                auto positionParams = pair.params;
                positionParams.applyVelocityImpulse = false;
                positionParams.applyFrictionImpulse = false;
                positionParams.applyPositionCorrection = true;
                const auto stats = collision::solveCollisionPair(
                    bodies_[pair.i], bodies_[pair.j], positionParams, false);
                metrics_.pairsVisited += stats.visited ? 1 : 0;
            }
        }

        for (int it = 0; it < velocityIterations; ++it) {
            for (auto& pair : activePairs) {
                auto velocityParams = pair.params;
                velocityParams.applyPositionCorrection = (it == 0 && positionIterations == 0);
                velocityParams.applyVelocityImpulse = true;
                velocityParams.applyFrictionImpulse = true;
                const auto stats = collision::solveCollisionPair(
                    bodies_[pair.i], bodies_[pair.j], velocityParams, false);
                metrics_.pairsVisited += stats.visited ? 1 : 0;
                metrics_.pairsImpulseApplied += stats.impulseApplied ? 1 : 0;
                if (stats.hasNormal && stats.normalImpulse > 0.0) {
                    pair.accumulatedImpulse += stats.normalImpulse;
                }
                if (stats.hasNormal) {
                    contactTouchedBodies_[pair.i] = true;
                    contactTouchedBodies_[pair.j] = true;
                    if (stats.impulseApplied) {
                        wakeBody_(bodies_[pair.i]);
                        wakeBody_(bodies_[pair.j]);
                    }
                }
            }
        }

        for (const auto& pair : activePairs) {
            if (!collision::isColliding(bodies_[pair.i], bodies_[pair.j])) {
                continue;
            }

            ContactManifold& manifold = contactCache_[pair.key];
            manifold.touched = true;
            manifold.staleFrames = 0;
            contactTouchedBodies_[pair.i] = true;
            contactTouchedBodies_[pair.j] = true;

            if (pair.accumulatedImpulse > 0.0) {
                manifold.normalImpulse = pair.accumulatedImpulse;
            } else {
                manifold.normalImpulse *= 0.9;
            }
            Vec3 normal{};
            if (collision::contactNormal(bodies_[pair.i], bodies_[pair.j], normal)) {
                manifold.normal = normal;
            }
        }
    }

    World::ContactKey World::contactKeyForPair_(const std::size_t i, const std::size_t j) const
    {
        const std::uint64_t a = bodies_[i].id;
        const std::uint64_t b = bodies_[j].id;
        return (a < b) ? ContactKey{a, b} : ContactKey{b, a};
    }

    collision::SolveParams World::solveParamsForPair_(const std::size_t i, const std::size_t j) const
    {
        const Body& a = bodies_[i];
        const Body& b = bodies_[j];

        collision::SolveParams params{};
        const double pairRestitution = std::clamp(std::min(a.restitution, b.restitution), 0.0, 1.0);
        const double worldRestitution = std::isfinite(params_.restitution)
            ? std::clamp(params_.restitution, 0.0, 1.0)
            : Params::kDefaultRestitution;
        params.restitution = std::min(pairRestitution, worldRestitution);
        params.staticFriction = std::sqrt(std::max(0.0, a.staticFriction) * std::max(0.0, b.staticFriction));
        params.dynamicFriction = std::sqrt(std::max(0.0, a.dynamicFriction) * std::max(0.0, b.dynamicFriction));
        if (params.dynamicFriction > params.staticFriction) {
            params.dynamicFriction = params.staticFriction;
        }
        params.penetrationSlop = params_.penetrationSlop;
        params.positionCorrectionPercent = params_.positionCorrectionPercent;
        return params;
    }

    void World::beginContactFrame_()
    {
        if (contactTouchedBodies_.size() != bodies_.size()) {
            contactTouchedBodies_.resize(bodies_.size());
        }
        std::fill(contactTouchedBodies_.begin(), contactTouchedBodies_.end(), false);
        for (auto& manifold : contactCache_ | std::views::values) {
            manifold.touched = false;
        }
    }

    void World::warmStartPairs_(const std::span<const ActiveCollisionPair> pairs)
    {
        for (const auto& pair : pairs) {
            constexpr double kWarmStartFactor = 0.85;
            auto it = contactCache_.find(pair.key);
            if (it == contactCache_.end()) {
                continue;
            }

            ContactManifold& manifold = it->second;
            if (manifold.normalImpulse <= 0.0) {
                continue;
            }
            if (!collision::isColliding(bodies_[pair.i], bodies_[pair.j])) {
                continue;
            }

            Vec3 normal = manifold.normal;
            if (!collision::contactNormal(bodies_[pair.i], bodies_[pair.j], normal)) {
                continue;
            }

            const double relativeNormalVelocity = (bodies_[pair.j].velocity - bodies_[pair.i].velocity).dot(normal);
            if (relativeNormalVelocity > 0.0) {
                continue;
            }

            collision::applyNormalImpulse(
                bodies_[pair.i], bodies_[pair.j], normal, manifold.normalImpulse * kWarmStartFactor);
            if (std::abs(manifold.tangentImpulse) > 1e-8) {
                Vec3 tangent = (bodies_[pair.j].velocity - bodies_[pair.i].velocity) - normal * relativeNormalVelocity;
                const double tangentLength = tangent.magnitude();
                if (tangentLength > 1e-8) {
                    tangent /= tangentLength;
                    collision::applyTangentImpulse(
                        bodies_[pair.i],
                        bodies_[pair.j],
                        normal,
                        tangent,
                        manifold.tangentImpulse * kWarmStartFactor);
                }
            }
            manifold.touched = true;
            manifold.staleFrames = 0;
            contactTouchedBodies_[pair.i] = true;
            contactTouchedBodies_[pair.j] = true;
            ++metrics_.warmStartedPairs;
        }
    }

    void World::endContactFrame_()
    {
        for (auto it = contactCache_.begin(); it != contactCache_.end();) {
            ContactManifold& manifold = it->second;
            if (manifold.touched) {
                manifold.staleFrames = 0;
                ++it;
                continue;
            }

            ++manifold.staleFrames;
            manifold.normalImpulse *= 0.75;
            manifold.tangentImpulse *= 0.6;
            if (manifold.staleFrames > 2 || manifold.normalImpulse < 1e-8) {
                it = contactCache_.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace sim

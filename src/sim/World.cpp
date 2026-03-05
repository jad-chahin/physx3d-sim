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

namespace sim {
    namespace {
        [[nodiscard]] double derivedInvInertiaSphere(const Body& b) {
            if (!std::isfinite(b.invMass) || b.invMass <= 0.0 || !std::isfinite(b.radius) || b.radius <= 0.0) {
                return 0.0;
            }
            return 2.5 * b.invMass / (b.radius * b.radius);
        }

        [[nodiscard]] double effectiveInvInertia(const Body& b) {
            if (std::isfinite(b.invInertia) && b.invInertia > 0.0) {
                return b.invInertia;
            }
            return derivedInvInertiaSphere(b);
        }
    } // namespace

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
        rebuildJointCollisionFilter_();
    }

    World::World(std::vector<Body> bodies, const Params& params)
        : params_(params), bodies_(std::move(bodies))
    {
        for (auto& b : bodies_) {
            assignBodyId_(b);
        }
        rebuildJointCollisionFilter_();
    }

    void World::step(const double dt)
    {
        metrics_ = {};
        beginContactFrame_();
        sanitizeBodies_();
        syncForces_();
        resetForces_();
        computeForces_();
        integrate_(dt);
        sanitizeBodies_();
        moveBodiesWithCCD_(dt);
        sanitizeBodies_();
        endContactFrame_();
        metrics_.manifoldActivePairs = contactCache_.size();
    }

    void World::addBody(const Body& b)
    {
        bodies_.push_back(b);
        assignBodyId_(bodies_.back());
        rebuildJointCollisionFilter_();
    }

    void World::addDistanceJoint(const DistanceJoint& joint)
    {
        if (joint.bodyA == joint.bodyB) {
            return;
        }
        if (joint.bodyA >= bodies_.size() || joint.bodyB >= bodies_.size()) {
            return;
        }

        DistanceJoint j = joint;
        if (!std::isfinite(j.restLength) || j.restLength < 0.0) {
            const Vec3 d = bodies_[j.bodyB].position - bodies_[j.bodyA].position;
            j.restLength = d.magnitude();
        }
        j.stiffness = std::clamp(j.stiffness, 0.0, 1.0);
        j.damping = std::clamp(j.damping, 0.0, 1.0);
        joints_.push_back(j);
        rebuildJointCollisionFilter_();
    }

    void World::clearJoints()
    {
        joints_.clear();
        rebuildJointCollisionFilter_();
    }

    void World::clear()
    {
        bodies_.clear();
        joints_.clear();
        forces_.clear();
        contactCache_.clear();
        nonCollidingConnectedPairs_.clear();
    }

    std::vector<Body>& World::bodies() { return bodies_; }
    const std::vector<Body>& World::bodies() const { return bodies_; }
    World::Params& World::params() { return params_; }
    const World::Params& World::params() const { return params_; }
    const World::Metrics& World::metrics() const { return metrics_; }
    const std::vector<World::DistanceJoint>& World::joints() const { return joints_; }

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

            const double invI = effectiveInvInertia(b);
            if (invI > 0.0) {
                const Vec3 alpha = b.torque * invI;
                b.angularVelocity = b.angularVelocity + alpha * dt;
            }
            b.torque = Vec3(0.0, 0.0, 0.0);
        }
    }

    void World::solveDistanceJoints_(const double dt)
    {
        if (joints_.empty() || dt <= 0.0 || params_.jointIterations <= 0) {
            return;
        }

        const double eps = std::numeric_limits<double>::epsilon();
        const double dtSafe = std::max(dt, eps);

        for (int it = 0; it < params_.jointIterations; ++it) {
            for (const DistanceJoint& joint : joints_) {
                if (joint.bodyA >= bodies_.size() || joint.bodyB >= bodies_.size()) {
                    continue;
                }

                Body& a = bodies_[joint.bodyA];
                Body& b = bodies_[joint.bodyB];
                if (a.invMass == 0.0 && b.invMass == 0.0) {
                    continue;
                }

                const Vec3 delta = b.position - a.position;
                const double dist = delta.magnitude();
                if (!std::isfinite(dist) || dist <= eps) {
                    continue;
                }

                const Vec3 n = delta / dist;
                const double invMassSum = a.invMass + b.invMass;
                if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
                    continue;
                }

                const double stretch = dist - joint.restLength;
                const double correctionMag = joint.stiffness * stretch;
                // Position solve: correct distance error directly to avoid explosive bias impulses.
                if (std::isfinite(correctionMag) && correctionMag != 0.0) {
                    const Vec3 correction = n * correctionMag;
                    a.position = a.position + correction * (a.invMass / invMassSum);
                    b.position = b.position - correction * (b.invMass / invMassSum);
                }

                // Velocity solve: damping only (no restorative energy injection).
                const double relSpeed = (b.velocity - a.velocity).dot(n);
                const double lambdaDamp = -(joint.damping * relSpeed) / invMassSum;
                if (std::isfinite(lambdaDamp) && lambdaDamp != 0.0) {
                    a.velocity = a.velocity - n * (lambdaDamp * a.invMass);
                    b.velocity = b.velocity + n * (lambdaDamp * b.invMass);
                }

                // Keep previous position aligned with corrected pose to avoid interpolation slingshots.
                a.prevPosition = a.position;
                b.prevPosition = b.position;
                ++metrics_.jointConstraintsSolved;
            }
        }
    }

    void World::advancePositions_(const double dt)
    {
        for (auto& b : bodies_) {
            b.position = b.position + b.velocity * dt;
            integrateOrientation(b.orientation, b.angularVelocity, dt);
        }
    }

    void World::sanitizeBodies_()
    {
        for (auto& b : bodies_) {
            bool bodySanitized = false;
            const auto mark = [&]() {
                ++metrics_.sanitizedFields;
                bodySanitized = true;
            };

            if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z)) {
                b.position = Vec3(0.0, 0.0, 0.0); mark();
            }
            if (!std::isfinite(b.prevPosition.x) || !std::isfinite(b.prevPosition.y) || !std::isfinite(b.prevPosition.z)) {
                b.prevPosition = b.position; mark();
            }
            if (!std::isfinite(b.velocity.x) || !std::isfinite(b.velocity.y) || !std::isfinite(b.velocity.z)) {
                b.velocity = Vec3(0.0, 0.0, 0.0); mark();
            }
            if (!std::isfinite(b.angularVelocity.x) || !std::isfinite(b.angularVelocity.y) || !std::isfinite(b.angularVelocity.z)) {
                b.angularVelocity = Vec3(0.0, 0.0, 0.0); mark();
            }
            if (!std::isfinite(b.torque.x) || !std::isfinite(b.torque.y) || !std::isfinite(b.torque.z)) {
                b.torque = Vec3(0.0, 0.0, 0.0); mark();
            }
            if (!std::isfinite(b.invMass) || b.invMass < 0.0) { b.invMass = 0.0; mark(); }
            if (!std::isfinite(b.invInertia) || b.invInertia < 0.0) { b.invInertia = 0.0; mark(); }
            if (!std::isfinite(b.radius) || b.radius <= 0.0) { b.radius = 1.0; mark(); }
            if (!std::isfinite(b.restitution)) { b.restitution = 0.5; mark(); }
            const double oldRest = b.restitution;
            b.restitution = std::clamp(b.restitution, 0.0, 1.0);
            if (b.restitution != oldRest) mark();
            if (!std::isfinite(b.staticFriction) || b.staticFriction < 0.0) { b.staticFriction = 0.6; mark(); }
            if (!std::isfinite(b.dynamicFriction) || b.dynamicFriction < 0.0) { b.dynamicFriction = 0.4; mark(); }
            if (b.dynamicFriction > b.staticFriction) { b.dynamicFriction = b.staticFriction; mark(); }
            if (!std::isfinite(b.orientation.w) || !std::isfinite(b.orientation.x) || !std::isfinite(b.orientation.y) || !std::isfinite(b.orientation.z)) {
                b.orientation = {}; mark();
            } else {
                normalizeQuat(b.orientation);
            }

            if (bodySanitized) {
                ++metrics_.sanitizedBodies;
            }
        }
    }

    void World::moveBodiesWithCCD_(const double dt)
    {
        if (!params_.enableCollisions) {
            solveDistanceJoints_(dt);
            advancePositions_(dt);
            return;
        }

        double remaining = dt;
        const double machineEps = std::numeric_limits<double>::epsilon();
        const double timeTol = machineEps * std::max(1.0, std::abs(dt));
        if (remaining <= timeTol) {
            return;
        }

        const auto pairs = broadphase::sweptPairs(bodies_, remaining);
        metrics_.broadphaseCandidatesSwept += pairs.size();

        bool found = false;
        double tHit = std::numeric_limits<double>::infinity();
        for (const auto& [i, j] : pairs) {
            const Body& A = bodies_[i];
            const Body& B = bodies_[j];
            if (A.invMass == 0.0 && B.invMass == 0.0) continue;
            if (!shouldCollidePair_(i, j)) continue;

            double t = 0.0;
            if (!collision::sweptCollisionTime(A, B, remaining, t)) continue;
            if (t < tHit) {
                tHit = t;
                found = true;
            }
        }

        if (!found) {
            solveDistanceJoints_(remaining);
            advancePositions_(remaining);
            remaining = 0.0;
        } else if (tHit > timeTol) {
            const double nextRemaining = remaining - tHit;
            if (nextRemaining < remaining) {
                solveDistanceJoints_(tHit);
                advancePositions_(tHit);
                remaining = nextRemaining;
            }
        }

        const auto overlapPairs = broadphase::discretePairs(bodies_);
        metrics_.broadphaseCandidatesDiscrete += overlapPairs.size();
        if (hasAnyOverlapInPairs_(overlapPairs)) {
            collidePairs_(overlapPairs, params_.collisionIterations);
            ++metrics_.ccdEvents;
            metrics_.ccdZeroTimePairsResolved += overlapPairs.size();
        }

        if (remaining > timeTol) {
            solveDistanceJoints_(remaining);
            advancePositions_(remaining);
        }
    }

    void World::collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, const int iterations)
    {
        if (pairs.empty() || iterations <= 0) {
            return;
        }

        std::vector<std::pair<std::size_t, std::size_t>> activePairs;
        activePairs.reserve(pairs.size());
        for (const auto& [i, j] : pairs) {
            if (shouldCollidePair_(i, j)) {
                activePairs.emplace_back(i, j);
            }
        }
        if (activePairs.empty()) {
            return;
        }

        warmStartPairs_(activePairs);

        std::unordered_map<ContactKey, double, World::PairHash> accumulatedImpulse;
        accumulatedImpulse.reserve(activePairs.size() * 2);
        std::vector<collision::SolveParams> pairParams;
        pairParams.reserve(activePairs.size());
        for (const auto& [i, j] : activePairs) {
            pairParams.push_back(solveParamsForPair_(i, j));
        }

        for (int it = 0; it < iterations; ++it) {
            for (std::size_t p = 0; p < activePairs.size(); ++p) {
                const auto& [i, j] = activePairs[p];
                const auto stats = collision::solveCollisionPair(bodies_[i], bodies_[j], pairParams[p], false);
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

        for (const auto& [i, j] : activePairs) {
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
                manifold.normalImpulse *= 0.9;
            }
        }
    }

    bool World::hasAnyOverlapInPairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs) const
    {
        for (const auto& [i, j] : pairs) {
            if (!shouldCollidePair_(i, j)) {
                continue;
            }
            if (collision::isColliding(bodies_[i], bodies_[j])) {
                return true;
            }
        }
        return false;
    }

    bool World::shouldCollidePair_(const std::size_t i, const std::size_t j) const
    {
        if (i == j || i >= bodies_.size() || j >= bodies_.size()) {
            return false;
        }
        const ContactKey key = contactKeyForPair_(i, j);
        return nonCollidingConnectedPairs_.find(key) == nonCollidingConnectedPairs_.end();
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
        params.restitution = std::clamp(std::min(a.restitution, b.restitution), 0.0, 1.0);
        params.staticFriction = std::sqrt(std::max(0.0, a.staticFriction) * std::max(0.0, b.staticFriction));
        params.dynamicFriction = std::sqrt(std::max(0.0, a.dynamicFriction) * std::max(0.0, b.dynamicFriction));
        if (params.dynamicFriction > params.staticFriction) {
            params.dynamicFriction = params.staticFriction;
        }
        params.penetrationSlop = params_.penetrationSlop;
        params.positionCorrectionPercent = params_.positionCorrectionPercent;
        return params;
    }

    void World::rebuildJointCollisionFilter_()
    {
        nonCollidingConnectedPairs_.clear();
        nonCollidingConnectedPairs_.reserve(joints_.size() * 2);
        for (const auto& joint : joints_) {
            if (joint.collideConnected) continue;
            if (joint.bodyA >= bodies_.size() || joint.bodyB >= bodies_.size()) continue;
            nonCollidingConnectedPairs_.insert(contactKeyForPair_(joint.bodyA, joint.bodyB));
        }
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
            if (!shouldCollidePair_(i, j)) continue;
            const ContactKey key = contactKeyForPair_(i, j);
            auto it = contactCache_.find(key);
            if (it == contactCache_.end()) continue;

            ContactManifold& manifold = it->second;
            if (manifold.normalImpulse <= 0.0) continue;
            if (!collision::isColliding(bodies_[i], bodies_[j])) continue;

            Vec3 n{};
            if (!collision::contactNormal(bodies_[i], bodies_[j], n)) continue;

            const double vN = (bodies_[j].velocity - bodies_[i].velocity).dot(n);
            if (vN > 0.0) continue;

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

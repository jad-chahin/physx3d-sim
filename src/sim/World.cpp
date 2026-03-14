#include "World.h"
#include "Broadphase.h"
#include "Collision.h"
#include "Material.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <utility>

namespace sim {
    namespace {
        [[nodiscard]] bool isDynamicBody(const Body& b) {
            return std::isfinite(b.invMass) && b.invMass > 0.0;
        }

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
        : World(std::move(bodies), Params{}) {}

    World::World(std::vector<Body> bodies, const Params& params)
        : params_(params), bodies_(std::move(bodies))
    {
        initBodies_();
    }

    void World::step(const double dt)
    {
        metrics_ = {};
        beginContactFrame_();
        sanitizeBodies_();
        prepareForces_();
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
        assignMaterial(bodies_.back(), bodies_.back().materialName);
        bodies_.back().prevPosition = bodies_.back().position;
        bodies_.back().prevOrientation = bodies_.back().orientation;
        assignBodyId_(bodies_.back());
    }

    void World::clear()
    {
        metrics_ = {};
        bodies_.clear();
        forces_.clear();
        contactCache_.clear();
        nextBodyId_ = 1;
    }

    std::vector<Body>& World::bodies() { return bodies_; }
    const std::vector<Body>& World::bodies() const { return bodies_; }
    World::Params& World::params() { return params_; }
    const World::Params& World::params() const { return params_; }
    const World::Metrics& World::metrics() const { return metrics_; }

    void World::prepareForces_()
    {
        if (bodies_.size() != forces_.size()) {
            forces_.resize(bodies_.size());
        }
        std::ranges::fill(forces_, Vec3{});
    }

    void World::computeForces_()
    {
        if (!params_.enableGravity || bodies_.size() < 2) {
            return;
        }

        thread_local std::vector<std::size_t> dynamicBodies;
        dynamicBodies.clear();
        dynamicBodies.reserve(bodies_.size());

        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            if (isDynamicBody(bodies_[i])) {
                dynamicBodies.push_back(i);
            }
        }

        if (dynamicBodies.size() < 2) {
            return;
        }

        for (std::size_t a = 0; a + 1 < dynamicBodies.size(); ++a) {
            const std::size_t i = dynamicBodies[a];
            for (std::size_t b = a + 1; b < dynamicBodies.size(); ++b) {
                applyGravityPair_(i, dynamicBodies[b]);
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
        forces_[i] += f12;
        forces_[j] -= f12;
    }

    void World::integrate_(const double dt)
    {
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            Body& b = bodies_[i];
            if (b.invMass == 0.0) {
                continue;
            }
            const Vec3 a = forces_[i] * b.invMass;
            b.velocity += a * dt;

            const double invI = effectiveInvInertia(b);
            if (invI > 0.0) {
                const Vec3 alpha = b.torque * invI;
                b.angularVelocity += alpha * dt;
            }
            b.torque = Vec3{};
        }
    }

    void World::advancePositions_(const double dt)
    {
        for (auto& b : bodies_) {
            b.position += b.velocity * dt;
            integrateOrientation(b.orientation, b.angularVelocity, dt);
        }
    }

    void World::sanitizeBodies_()
    {
        const Material& fallbackMaterial = defaultMaterial();

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
            if (b.materialName.empty()) {
                assignMaterial(b, kDefaultMaterialName);
                mark();
            }
            if (!std::isfinite(b.restitution)) { b.restitution = fallbackMaterial.restitution; mark(); }
            const double oldRest = b.restitution;
            b.restitution = std::clamp(b.restitution, 0.0, 1.0);
            if (b.restitution != oldRest) mark();
            if (!std::isfinite(b.staticFriction) || b.staticFriction < 0.0) { b.staticFriction = fallbackMaterial.staticFriction; mark(); }
            if (!std::isfinite(b.dynamicFriction) || b.dynamicFriction < 0.0) { b.dynamicFriction = fallbackMaterial.dynamicFriction; mark(); }
            if (b.dynamicFriction > b.staticFriction) { b.dynamicFriction = b.staticFriction; mark(); }
            if (!std::isfinite(b.orientation.w) || !std::isfinite(b.orientation.x) || !std::isfinite(b.orientation.y) || !std::isfinite(b.orientation.z)) {
                b.orientation = {}; mark();
            } else {
                normalizeQuat(b.orientation);
            }
            if (!std::isfinite(b.prevOrientation.w) || !std::isfinite(b.prevOrientation.x) ||
                !std::isfinite(b.prevOrientation.y) || !std::isfinite(b.prevOrientation.z))
            {
                b.prevOrientation = b.orientation; mark();
            } else {
                normalizeQuat(b.prevOrientation);
            }

            if (bodySanitized) {
                ++metrics_.sanitizedBodies;
            }
        }
    }

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

        while (remaining > timeTol) {
            const auto pairs = broadphase::sweptPairs(bodies_, remaining);
            metrics_.broadphaseCandidatesSwept += pairs.size();

            bool found = false;
            double tHit = std::numeric_limits<double>::infinity();
            std::size_t toiI = 0;
            std::size_t toiJ = 0;
            for (const auto& [i, j] : pairs) {
                const Body& A = bodies_[i];
                const Body& B = bodies_[j];
                if (A.invMass == 0.0 && B.invMass == 0.0) continue;

                double t = 0.0;
                if (!collision::sweptCollisionTime(A, B, remaining, t)) continue;
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
                }
            }

            const auto toiStats = collision::solveCollisionPair(
                bodies_[toiI], bodies_[toiJ], solveParamsForPair_(toiI, toiJ), true);
            metrics_.pairsVisited += toiStats.visited ? 1 : 0;
            metrics_.pairsImpulseApplied += toiStats.impulseApplied ? 1 : 0;

            if (toiStats.hasNormal) {
                ContactManifold& manifold = contactCache_[contactKeyForPair_(toiI, toiJ)];
                manifold.touched = true;
                manifold.staleFrames = 0;
                if (toiStats.normalImpulse > 0.0) {
                    manifold.normalImpulse = toiStats.normalImpulse;
                } else {
                    manifold.normalImpulse *= 0.9;
                }
            }
            ++metrics_.ccdEvents;

            const auto overlapPairs = broadphase::discretePairs(bodies_);
            metrics_.broadphaseCandidatesDiscrete += overlapPairs.size();
            std::vector<std::pair<std::size_t, std::size_t>> zeroTimeOverlapPairs;
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
                collidePairs_(zeroTimeOverlapPairs, params_.collisionIterations);
                ++metrics_.ccdEvents;
                if (params_.collisionIterations > 0) {
                    metrics_.ccdZeroTimePairsResolved += zeroTimeOverlapPairs.size();
                }
            }

            if (!advancedToToi) {
                // Ensure forward progress for repeated zero-time TOI contacts.
                const double minConsume = std::max(timeTol * 32.0, std::max(0.0, dt) / 128.0);
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

    void World::collidePairs_(const std::vector<std::pair<std::size_t, std::size_t>>& pairs, const int iterations)
    {
        if (pairs.empty() || iterations <= 0) {
            return;
        }

        std::vector<ActiveCollisionPair> activePairs;
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

        for (int it = 0; it < iterations; ++it) {
            for (auto& pair : activePairs) {
                const auto stats = collision::solveCollisionPair(
                    bodies_[pair.i], bodies_[pair.j], pair.params, false);
                metrics_.pairsVisited += stats.visited ? 1 : 0;
                metrics_.pairsImpulseApplied += stats.impulseApplied ? 1 : 0;

                if (stats.hasNormal && stats.normalImpulse > 0.0) {
                    pair.accumulatedImpulse += stats.normalImpulse;
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

            if (pair.accumulatedImpulse > 0.0) {
                manifold.normalImpulse = pair.accumulatedImpulse;
            } else {
                manifold.normalImpulse *= 0.9;
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

    void World::assignBodyId_(Body& b)
    {
        b.id = nextBodyId_++;
    }

    void World::initBodies_()
    {
        for (auto& body : bodies_) {
            assignMaterial(body, body.materialName);
            body.prevPosition = body.position;
            body.prevOrientation = body.orientation;
            assignBodyId_(body);
        }
    }

    void World::beginContactFrame_()
    {
        for (auto &val: contactCache_ | std::views::values) {
            val.touched = false;
        }
    }

    void World::warmStartPairs_(const std::span<const ActiveCollisionPair> pairs)
    {
        for (const auto& pair : pairs) {
            constexpr double kWarmStartFactor = 0.85;
            auto it = contactCache_.find(pair.key);
            if (it == contactCache_.end()) continue;

            ContactManifold& manifold = it->second;
            if (manifold.normalImpulse <= 0.0) continue;
            if (!collision::isColliding(bodies_[pair.i], bodies_[pair.j])) continue;

            Vec3 n{};
            if (!collision::contactNormal(bodies_[pair.i], bodies_[pair.j], n)) continue;

            const double vN = (bodies_[pair.j].velocity - bodies_[pair.i].velocity).dot(n);
            if (vN > 0.0) continue;

            collision::applyNormalImpulse(
                bodies_[pair.i], bodies_[pair.j], n, manifold.normalImpulse * kWarmStartFactor);
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

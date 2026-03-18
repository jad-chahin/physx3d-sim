#include "World.h"
#include "Material.h"

#include <algorithm>
#include <cmath>
#include <ranges>

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
        const bool manageDiagnosticsInternally = !diagnosticsManagedExternally_;
        if (manageDiagnosticsInternally) {
            beginDiagnostics();
        }
        const int substeps = computeSubstepCount_(dt);
        const double substepDt = dt / static_cast<double>(substeps);
        for (int substep = 0; substep < substeps; ++substep) {
            stepSingle_(substepDt);
        }
        if (manageDiagnosticsInternally) {
            finalizeDiagnostics();
        }
    }

    void World::beginDiagnostics()
    {
        diagnosticsManagedExternally_ = true;
        metrics_ = {};
    }

    void World::finalizeDiagnostics()
    {
        metrics_.manifoldActivePairs = contactCache_.size();
        diagnosticsManagedExternally_ = false;
    }

    void World::stepSingle_(const double dt)
    {
        beginContactFrame_();
        prepareForces_();
        computeForces_();
        integrateVelocities_(dt * 0.5);
        moveBodiesWithCCD_(dt);
        sanitizeBodies_();
        prepareForces_();
        computeForces_();
        integrateVelocities_(dt * 0.5);
        sanitizeBodies_();
        updateSleepState_(dt);
        endContactFrame_();
    }

    void World::addBody(const Body& b)
    {
        bodies_.push_back(b);
        assignMaterial(bodies_.back(), bodies_.back().materialName);
        bodies_.back().prevPosition = bodies_.back().position;
        bodies_.back().prevOrientation = bodies_.back().orientation;
        bodies_.back().sleeping = false;
        bodies_.back().sleepTimer = 0.0;
        assignBodyId_(bodies_.back());
        sanitizeBody_(bodies_.back());
    }

    void World::clear()
    {
        metrics_ = {};
        bodies_.clear();
        forces_.clear();
        contactTouchedBodies_.clear();
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
        if (bodies_.size() != contactTouchedBodies_.size()) {
            contactTouchedBodies_.resize(bodies_.size());
        }
        std::ranges::fill(forces_, Vec3{});
    }

    int World::computeSubstepCount_(const double dt) const
    {
        const double absDt = std::abs(dt);
        if (!(absDt > 0.0)) {
            return 1;
        }

        const double maxSubstepDt =
            (std::isfinite(params_.maxSubstepDt) && params_.maxSubstepDt > 0.0)
                ? params_.maxSubstepDt
                : Params::kDefaultMaxSubstepDt;
        const int maxSubsteps = std::clamp(params_.maxSubsteps, 1, Params::kDefaultMaxSubsteps);
        const int requested = static_cast<int>(std::ceil(absDt / maxSubstepDt));
        return std::clamp(requested, 1, maxSubsteps);
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
        metrics_.gravityBodies += dynamicBodies.size();

        if (dynamicBodies.size() < 2) {
            return;
        }

        for (std::size_t a = 0; a + 1 < dynamicBodies.size(); ++a) {
            const std::size_t i = dynamicBodies[a];
            for (std::size_t b = a + 1; b < dynamicBodies.size(); ++b) {
                ++metrics_.gravityPairs;
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

        const double invMassProduct = A.invMass * B.invMass;
        if (!std::isfinite(invMassProduct) || invMassProduct <= 0.0) {
            return;
        }

        const double invR = 1.0 / std::sqrt(r2Soft);
        const double invR3 = invR * invR * invR;
        const double forceScale = (params_.G / invMassProduct) * invR3;
        const Vec3 f12 = d * forceScale;
        forces_[i] += f12;
        forces_[j] -= f12;
    }

    void World::integrateVelocities_(const double dt)
    {
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            Body& b = bodies_[i];
            if (b.invMass == 0.0 || b.sleeping) {
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
            if (b.sleeping) {
                continue;
            }
            b.position += b.velocity * dt;
            integrateOrientation(b.orientation, b.angularVelocity, dt);
        }
    }

    void World::sanitizeBody_(Body& b)
    {
        const Material& fallbackMaterial = defaultMaterial();

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
        if (b.invMass <= 0.0) {
            b.sleeping = false;
            b.sleepTimer = 0.0;
        } else if (!std::isfinite(b.sleepTimer) || b.sleepTimer < 0.0) {
            b.sleepTimer = 0.0;
            b.sleeping = false;
            mark();
        }

        if (bodySanitized) {
            ++metrics_.sanitizedBodies;
        }
    }

    void World::sanitizeBodies_()
    {
        for (auto& b : bodies_) {
            sanitizeBody_(b);
        }
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
            body.sleeping = false;
            body.sleepTimer = 0.0;
            assignBodyId_(body);
            sanitizeBody_(body);
        }
    }

    void World::wakeBody_(Body& b)
    {
        if (!isDynamicBody(b)) {
            return;
        }
        b.sleeping = false;
        b.sleepTimer = 0.0;
    }

    void World::updateSleepState_(const double dt)
    {
        if (!params_.enableSleeping) {
            for (auto& body : bodies_) {
                wakeBody_(body);
            }
            return;
        }

        const double linearThreshold = std::max(0.0, params_.sleepLinearThreshold);
        const double angularThreshold = std::max(0.0, params_.sleepAngularThreshold);
        const double linearThreshold2 = linearThreshold * linearThreshold;
        const double angularThreshold2 = angularThreshold * angularThreshold;
        const double requiredSleepTime = std::max(0.0, params_.sleepTime);

        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            Body& body = bodies_[i];
            if (!isDynamicBody(body)) {
                continue;
            }

            const bool touching = i < contactTouchedBodies_.size() && contactTouchedBodies_[i];
            const bool eligibleForSleep = !params_.enableGravity || touching;
            const double linearSpeed2 = body.velocity.dot(body.velocity);
            const double angularSpeed2 = body.angularVelocity.dot(body.angularVelocity);
            const bool belowThresholds =
                linearSpeed2 <= linearThreshold2 &&
                angularSpeed2 <= angularThreshold2;

            if (eligibleForSleep && belowThresholds) {
                body.sleepTimer += dt;
                if (body.sleepTimer >= requiredSleepTime) {
                    body.sleeping = true;
                    body.velocity = Vec3{};
                    body.angularVelocity = Vec3{};
                }
            } else {
                body.sleepTimer = 0.0;
                body.sleeping = false;
            }
        }
    }

} // namespace sim

//
// Created by jchah on 2025-12-19.
//

#include "World.h"

#include <algorithm>
#include <cmath>    // sqrt
#include <cstddef>  // std::size_t
#include <utility>  // std::move

namespace sim {
    // Constructors
    World::World(const Params &params)
    : params_(params) {}

    World::World(std::vector<Body> bodies)
    : bodies_(std::move(bodies)) {}

    World::World(std::vector<Body> bodies, const Params &params)
    : params_(params), bodies_(std::move(bodies)) {}

    void World::step(double dt)
    {
        syncForces_();
        resetForces_();
        computeForces_();
        integrate_(dt);
        collide_();
    }

    void World::addBody(const Body& b)
    {
        bodies_.push_back(b);
    }

    void World::clear()
    {
        bodies_.clear();
        forces_.clear();
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

    void World::syncForces_()
    {
        if (bodies_.size() != forces_.size()) {
            forces_.resize(bodies_.size());
        }
    }

    void World::resetForces_()
    {
        for (auto & force : forces_) {
            force = Vec3(0.0, 0.0, 0.0);
        }
    }

    void World::computeForces_()
    {
        if (params_.enableGravity) {
            // Compute effect of gravity on all unique pairs
            for (std::size_t i = 0; i < bodies_.size(); i++) {
                for (std::size_t j = i + 1; j < bodies_.size(); j++) {
                    applyGravityPair_(i, j);
                }
            }
        }
    }

    double World::epsilon(std::size_t i, std::size_t j) const {
        // Scale epsilon softening with radius sizes
        return (bodies_[i].radius + bodies_[j].radius) * 1e-6;
    }

    void World::applyGravityPair_(std::size_t i, std::size_t j)
    {
        Body& A = bodies_[i];
        Body& B = bodies_[j];

        double invM1 = A.invMass;
        double invM2 = B.invMass;

        if (invM1 == 0 or invM2 == 0) {
            return; // No gravity for static objects by convention
        }

        Vec3 d = B.position - A.position; // Difference vector

        double r1 = d.dot(d); // r^2

        double eps = epsilon(i, j);
        double r1s = r1 + eps * eps; // r^2 + eps^2 (softened)

        double m1 = 1.0 / invM1;
        double m2 = 1.0 / invM2;

        double inv_r  = 1.0 / std::sqrt(r1s);
        double inv_r3 = inv_r * inv_r * inv_r;

        Vec3 F12 = d * (params_.G * m1 * m2 * inv_r3);

        forces_[i] = forces_[i] + F12;
        forces_[j] = forces_[j] - F12;
    }

    void World::integrate_(double dt)
    {
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            Body& b = bodies_[i];
            if (b.invMass == 0) {
                continue; // Forces won't affect static objects
            }
            Vec3 a = forces_[i] * b.invMass; // a = F/m
            b.velocity = b.velocity + a * dt;
            b.position = b.position + b.velocity * dt;
        }
    }

    void World::collide_()
    {
        if (!params_.enableCollisions) return;
        // Solve unique collision pairs collisionIterations times
        for (int i = 0; i < params_.collisionIterations; ++i) {
            for (std::size_t j = 0; j < bodies_.size(); ++j) {
                for (std::size_t k = j + 1; k < bodies_.size(); ++k) {
                    solveCollisionPair_(j, k);
                }
            }
        }
    }

    bool World::isColliding_(std::size_t i, std::size_t j) {
        Body& A = bodies_[i];
        Body& B = bodies_[j];
        Vec3 d = B.position - A.position; // Difference vector
        double minDistance = A.radius + B.radius;
        double r = d.dot(d); // r^2

        return (r <= minDistance * minDistance); // r^2 <= minDistance^2 ?
    }

    void World::solveCollisionPair_(std::size_t i, std::size_t j)
    {
        if (!isColliding_(i, j)) {
            return;
        }

        Body& A = bodies_[i];
        Body& B = bodies_[j];
        double wA = A.invMass;
        double wB = B.invMass;

        if (wA == 0 and wB == 0) {
            return; // Both static -> do nothing
        }

        Vec3& pA = A.position;
        Vec3& pB = B.position;
        Vec3& vA = A.velocity;
        Vec3& vB = B.velocity;
        double e = params_.restitution;

        Vec3 d = pB - pA; // Difference vector
        double dist = d.magnitude(); // Distance

        double pen = (A.radius + B.radius) - dist; // Penetration depth (how much they overlap)

        Vec3 n; // Collision normal

        if (dist < epsilon(i, j)){
            n = Vec3(1.0, 0.0, 0.0); // Default to +x-axis if dist is ~0
        } else {
            n = d / dist;
        }

        double invMassSum = wA + wB;

        // Position-correction amount: ignore small overlap (slop), then apply a percent of the remaining penetration
        double correction = std::max(0.0, pen - params_.penetrationSlop) * params_.positionCorrectionPercent;

        // Move positions apart along collision normal in proportion to mass
        // Higher invMass => lower mass => larger change in position
        pA = pA - n * (correction * wA/invMassSum);
        pB = pB + n * (correction * wB/invMassSum);

        double v_n = (vB - vA).dot(n); // Speed of B relative to A along collision normal

        if (v_n >= 0) {
            return; // Already separating along the normal -> don't apply impulse
        }

        double impulse = -(1 + e) * v_n / invMassSum; // Normal impulse magnitude

        Vec3 J = n * impulse; // Impulse vector along collision normal

        // Impulse/mass = change in velocity
        vA = vA - J * wA;
        vB = vB + J * wB;
    }
} // sim


// TODO: For future:
/*
 * Swap integrator to leapfrog / velocity Verlet
 * Use fixed timestep + substeps (especially for collisions)
 * Add continuous collision detection (time-of-impact) to prevent tunneling
 * Upgrade collision solving: more iterations + better position correction / stable contacts
 * Add angular velocity + torque (spin from collisions)
 */
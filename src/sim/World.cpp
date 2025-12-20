//
// Created by jchah on 2025-12-19.
//

/*
 * 1. Done
 * 2. Done
 * 3. Done
 * 4. Done
 * 5. Done
 * 6. solveCollisionPair_
 * 7. step (just call the above in order)
 *
 * Finished:
 * - addBody, clear, bodies()
 * - ensureBuffers_
 * - resetForces_
 * - computeForces_, applyGravityPair_
 * - integrate_
 * - collide_
 *
 */

#include "World.h"

#include <algorithm>
#include <cmath>    // sqrt
#include <cstddef>  // std::size_t
#include <utility>  // std::move

namespace sim {
    World::World(const Params &params)
    : params_(params) {}

    World::World(std::vector<Body> bodies, const Params &params)
    : params_(params), bodies_(std::move(bodies)) {}

    void World::step(double dt)
    {
        (void)dt;

        // TODO: 0) Ensure dt is sensible (e.g., dt > 0).
        // TODO: 1) ensureBuffers_() so forces_ matches bodies_.size().
        // TODO: 2) resetForces_() (set net force to {0,0,0} for each body).
        // TODO: 3) computeForces_() (gravity for now; skip static bodies as needed).
        // TODO: 4) integrate_(dt) (update velocity then position; skip invMass==0).
        // TODO: 5) collide_(dt) (detect + resolve sphere-sphere overlaps/impulses).
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

    void World::ensureBuffers_()
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
            for (std::size_t i = 0; i < bodies_.size(); i++) {
                for (std::size_t j = i + 1; j < bodies_.size(); j++) {
                    applyGravityPair_(i, j);
                }
            }
        }
    }

    void World::applyGravityPair_(std::size_t i, std::size_t j)
    {
        double invM1 = bodies_[i].invMass;
        double invM2 = bodies_[j].invMass;

        if (invM1 == 0 or invM2 == 0) { // No gravity for static objects by convention
            return;
        }

        Vec3 d = bodies_[j].position - bodies_[i].position;

        double r1 = d.dot(d);
        double R  = bodies_[i].radius + bodies_[j].radius;

        double eps = 1e-6 * R;
        double r1s = r1 + eps * eps; // softened r^2

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
                continue;
            }
            Vec3 a = forces_[i] * b.invMass;
            b.velocity = b.velocity + a * dt;
            b.position = b.position + b.velocity * dt;
        }
    }

    void World::collide_()
    {
        if (!params_.enableCollisions) return;

        for (int i = 0; i < params_.collisionIterations; ++i) {
            for (std::size_t j = 0; j < bodies_.size(); ++j) {
                for (std::size_t k = j + 1; k < bodies_.size(); ++k) {
                    solveCollisionPair_(j, k);
                }
            }
        }
    }

    void World::solveCollisionPair_(std::size_t i, std::size_t j)
    {
        (void)i; (void)j;

        // TODO: Let A = bodies_[i], B = bodies_[j].
        // TODO: Sphere-sphere detection:
        //   - d = B.position - A.position
        //   - dist2 = dot(d,d)
        //   - dist2 = dot(d,d)
        //   - r = A.radius + B.radius
        //   - if (dist2 >= r*r): return (no overlap)
        //
        // TODO: Compute collision normal n:
        //   - dist = sqrt(dist2)
        //   - if dist is ~0: pick any stable normal (e.g. {1,0,0})
        //   - else n = d / dist
        //
        // TODO: Penetration depth:
        //   - penetration = r - dist
        //
        // TODO: Position correction (to separate overlaps):
        //   - invA = A.invMass, invB = B.invMass, invSum = invA + invB
        //   - if invSum == 0: return (both static)
        //   - correctedPen = max(0, penetration - params_.penetrationSlop)
        //   - move = n * (correctedPen * params_.positionCorrectionPercent / invSum)
        //   - if invA != 0: A.position -= move * invA
        //   - if invB != 0: B.position += move * invB
        //
        // TODO: Velocity impulse (bounce):
        //   - relativeVel = B.velocity - A.velocity
        //   - vn = dot(relativeVel, n)   (normal component)
        //   - if vn > 0: return (already separating; you might still keep position correction)
        //   - e = params_.restitution
        //   - j = -(1 + e) * vn / invSum
        //   - impulse = n * j
        //   - if invA != 0: A.velocity -= impulse * invA
        //   - if invB != 0: B.velocity += impulse * invB
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
//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_BODY_H
#define PHYSICS3D_BODY_H

#include <cstdint>
#include "Quaternion.h"
#include "Vec3.h"


namespace sim {

    class Body {
        public:
        Vec3 position; // [m]
        Vec3 velocity; // [m/s]
        Vec3 angularVelocity; // [rad/s]
        Vec3 torque; // [N*m] accumulated external torque for this step
        double invMass{}; // [1/kg]  (0 means "infinite mass" / static body)
        double invInertia{0.0}; // [1/(kg*m^2)] if <=0, sphere inertia is derived from mass/radius
        double radius{1.0}; // [m]
        double restitution{0.5}; // [0..1] bounciness
        double staticFriction{0.6}; // Coulomb static friction coefficient
        double dynamicFriction{0.4}; // Coulomb dynamic friction coefficient
        Quaternion orientation; // world orientation
        Vec3 prevPosition;
        std::uint64_t id = 0; // Stable identity assigned by World
    };
} // sim

#endif //PHYSICS3D_BODY_H

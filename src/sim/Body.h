//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_BODY_H
#define PHYSICS3D_BODY_H
#include "Vec3.h"

namespace sim {

    class Body {
        public:
        Vec3 position; // [m]
        Vec3 velocity; // [m/s]
        double invMass{}; // [1/kg]  (0 means "infinite mass" / static body)
        double radius{1.0}; // [m]
        Vec3 prevPosition;
    };
} // sim

#endif //PHYSICS3D_BODY_H
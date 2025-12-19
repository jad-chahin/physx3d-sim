//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_VEC3_H
#define PHYSICS3D_VEC3_H

namespace sim {

    class Vec3 {
        public:
        double x{}, y{}, z{};

        Vec3 operator+(const Vec3& o) const {
            return {x + o.x, y + o.y, z + o.z};
        }
        Vec3 operator*(double s) const {
            return {x * s, y * s, z * s};
        }
    };
} // sim

#endif //PHYSICS3D_VEC3_H
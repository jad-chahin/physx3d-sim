//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_VEC3_H
#define PHYSICS3D_VEC3_H

#include <cmath>


namespace sim {

    class Vec3 {
        public:
        double x{}, y{}, z{};

        Vec3 operator+(const Vec3& o) const {
            return {x + o.x, y + o.y, z + o.z};
        }

        Vec3 operator-(const Vec3& o) const {
            return {x - o.x, y - o.y, z - o.z};
        }


        Vec3 operator*(double s) const {
            return {x * s, y * s, z * s};
        }

        Vec3 operator/(double s) const {
            return {x / s, y / s, z / s};
        }

        [[nodiscard]] double dot(const Vec3& o) const {
            return x * o.x + y * o.y + z * o.z;
        }

        [[nodiscard]] double magnitude() const {
            return sqrt(x * x + y * y + z * z);
        }

        [[nodiscard]] Vec3 cross(const Vec3& o) const {
            return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
        }


        Vec3() = default;

        Vec3(double xIn, double yIn, double zIn)
        {
            x = xIn;
            y = yIn;
            z = zIn;
        }

    };
} // sim

#endif //PHYSICS3D_VEC3_H
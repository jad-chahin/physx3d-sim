//
// Created by jchah on 2025-12-19.
//

#ifndef PHYSICS3D_VEC3_H
#define PHYSICS3D_VEC3_H

#include <cmath>

namespace sim {
    struct Vec3 {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        constexpr Vec3() = default;
        constexpr Vec3(const double xIn, const double yIn, const double zIn)
            : x(xIn), y(yIn), z(zIn) {}

        [[nodiscard]] constexpr Vec3 operator+(const Vec3& o) const {
            return {x + o.x, y + o.y, z + o.z};
        }

        [[nodiscard]] constexpr Vec3 operator-(const Vec3& o) const {
            return {x - o.x, y - o.y, z - o.z};
        }

        [[nodiscard]] constexpr Vec3 operator*(const double s) const {
            return {x * s, y * s, z * s};
        }

        [[nodiscard]] constexpr Vec3 operator/(const double s) const {
            return {x / s, y / s, z / s};
        }

        constexpr Vec3& operator+=(const Vec3& o) {
            x += o.x;
            y += o.y;
            z += o.z;
            return *this;
        }

        constexpr Vec3& operator-=(const Vec3& o) {
            x -= o.x;
            y -= o.y;
            z -= o.z;
            return *this;
        }

        constexpr Vec3& operator*=(const double s) {
            x *= s;
            y *= s;
            z *= s;
            return *this;
        }

        constexpr Vec3& operator/=(const double s) {
            x /= s;
            y /= s;
            z /= s;
            return *this;
        }

        [[nodiscard]] constexpr double dot(const Vec3& o) const {
            return x * o.x + y * o.y + z * o.z;
        }

        [[nodiscard]] double magnitude() const {
            return std::sqrt(dot(*this));
        }

        [[nodiscard]] constexpr Vec3 cross(const Vec3& o) const {
            return {
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x
            };
        }
    };
} // namespace sim

#endif // PHYSICS3D_VEC3_H

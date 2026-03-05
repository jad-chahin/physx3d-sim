//
// Minimal quaternion utilities for body orientation integration.
//

#ifndef PHYSICS3D_QUATERNION_H
#define PHYSICS3D_QUATERNION_H

#include <cmath>
#include "Vec3.h"

namespace sim {
    struct Quaternion {
        double w = 1.0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    [[nodiscard]] inline Quaternion quatMultiply(const Quaternion& a, const Quaternion& b) {
        return {
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
        };
    }

    inline void normalizeQuat(Quaternion& q) {
        const double n2 = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
        if (n2 <= 0.0) {
            q = {};
            return;
        }
        const double invN = 1.0 / std::sqrt(n2);
        q.w *= invN;
        q.x *= invN;
        q.y *= invN;
        q.z *= invN;
    }

    inline void integrateOrientation(Quaternion& q, const Vec3& angularVelocity, const double dt) {
        const Quaternion omegaQ{0.0, angularVelocity.x, angularVelocity.y, angularVelocity.z};
        const Quaternion dq = quatMultiply(omegaQ, q);
        q.w += 0.5 * dt * dq.w;
        q.x += 0.5 * dt * dq.x;
        q.y += 0.5 * dt * dq.y;
        q.z += 0.5 * dt * dq.z;
        normalizeQuat(q);
    }
} // namespace sim

#endif // PHYSICS3D_QUATERNION_H

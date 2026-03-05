//
// Collision helpers extracted from World orchestrator.
//

#ifndef PHYSICS3D_COLLISION_H
#define PHYSICS3D_COLLISION_H

#include "Body.h"

namespace sim::collision {

    struct SolveParams {
        double restitution = 0.5;
        double staticFriction = 0.6;
        double dynamicFriction = 0.4;
        double penetrationSlop = 1e-4;
        double positionCorrectionPercent = 0.8;
    };

    struct SolveStats {
        bool visited = false;
        bool impulseApplied = false;
        bool hasNormal = false;
        Vec3 normal{};
        double normalImpulse = 0.0;
    };

    [[nodiscard]] bool isColliding(const Body& a, const Body& b);
    [[nodiscard]] bool contactNormal(const Body& a, const Body& b, Vec3& outNormal);
    [[nodiscard]] bool sweptCollisionTime(const Body& a, const Body& b, double maxTime, double& outTime);
    void applyNormalImpulse(Body& a, Body& b, const Vec3& normal, double impulse);
    void applyTangentImpulse(Body& a, Body& b, const Vec3& normal, const Vec3& tangent, double impulse);
    SolveStats solveCollisionPair(Body& a, Body& b, const SolveParams& params, bool assumeColliding);

} // namespace sim::collision

#endif // PHYSICS3D_COLLISION_H

//
// Collision helpers extracted from World orchestrator.
//

#ifndef PHYSICS3D_COLLISION_H
#define PHYSICS3D_COLLISION_H

#include "Body.h"

namespace sim::collision {

    struct SolveParams {
        double restitution = 0.5;
        double penetrationSlop = 1e-4;
        double positionCorrectionPercent = 0.8;
    };

    struct SolveStats {
        bool visited = false;
        bool impulseApplied = false;
    };

    [[nodiscard]] bool isColliding(const Body& a, const Body& b);
    [[nodiscard]] bool sweptCollisionTime(const Body& a, const Body& b, double maxTime, double& outTime);
    SolveStats solveCollisionPair(Body& a, Body& b, const SolveParams& params, bool assumeColliding);

} // namespace sim::collision

#endif // PHYSICS3D_COLLISION_H

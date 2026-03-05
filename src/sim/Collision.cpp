//
// Collision helpers extracted from World orchestrator.
//

#include "Collision.h"
#include <algorithm>
#include <cmath>

namespace sim::collision {
    namespace {
        [[nodiscard]] double epsilon(const Body& a, const Body& b) {
            return std::max((a.radius + b.radius) * 1e-6, 1e-12);
        }
    } // namespace

    bool isColliding(const Body& a, const Body& b)
    {
        const Vec3 d = b.position - a.position;
        const double minDistance = a.radius + b.radius;
        const double r2 = d.dot(d);
        return r2 <= minDistance * minDistance;
    }

    bool contactNormal(const Body& a, const Body& b, Vec3& outNormal)
    {
        const Vec3 d = b.position - a.position;
        const double dist2 = d.dot(d);
        if (!std::isfinite(dist2) || dist2 < 0.0) {
            return false;
        }

        const double dist = std::sqrt(dist2);
        const double eps = epsilon(a, b);
        if (dist < eps) {
            outNormal = Vec3(1.0, 0.0, 0.0);
            return true;
        }

        outNormal = d / dist;
        return std::isfinite(outNormal.x) && std::isfinite(outNormal.y) && std::isfinite(outNormal.z);
    }

    bool sweptCollisionTime(const Body& a, const Body& b, const double maxTime, double& outTime)
    {
        const Vec3 p = b.position - a.position;
        const Vec3 v = b.velocity - a.velocity;
        const double r = a.radius + b.radius;

        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            !std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
            !std::isfinite(r) || r < 0.0 || !std::isfinite(maxTime) || maxTime < 0.0) {
            return false;
        }

        const double c = p.dot(p) - r * r;
        if (c <= 0.0) {
            outTime = 0.0;
            return true;
        }

        const double aCoef = v.dot(v);
        if (aCoef <= 1e-14 || !std::isfinite(aCoef)) {
            return false;
        }

        const double bCoef = 2.0 * p.dot(v);
        const double disc = bCoef * bCoef - 4.0 * aCoef * c;
        if (disc < 0.0 || !std::isfinite(disc)) {
            return false;
        }

        const double sqrtDisc = std::sqrt(disc);
        const double invDenom = 0.5 / aCoef;
        const double t0 = (-bCoef - sqrtDisc) * invDenom;
        const double t1 = (-bCoef + sqrtDisc) * invDenom;

        if (t1 < 0.0) {
            return false;
        }

        const double tHit = (t0 >= 0.0) ? t0 : 0.0;
        if (tHit > maxTime) {
            return false;
        }

        outTime = tHit;
        return true;
    }

    void applyNormalImpulse(Body& a, Body& b, const Vec3& normal, const double impulse)
    {
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) ||
            !std::isfinite(impulse) || impulse <= 0.0) {
            return;
        }

        const double wA = a.invMass;
        const double wB = b.invMass;
        if (!std::isfinite(wA) || !std::isfinite(wB) || (wA == 0.0 && wB == 0.0)) {
            return;
        }

        const Vec3 j = normal * impulse;
        a.velocity = a.velocity - j * wA;
        b.velocity = b.velocity + j * wB;
    }

    SolveStats solveCollisionPair(Body& a, Body& b, const SolveParams& params, const bool assumeColliding)
    {
        SolveStats stats{};
        stats.visited = true;

        if (!assumeColliding && !isColliding(a, b)) {
            return stats;
        }

        const double wA = a.invMass;
        const double wB = b.invMass;
        if (!std::isfinite(wA) || !std::isfinite(wB) || (wA == 0.0 && wB == 0.0)) {
            return stats;
        }

        Vec3& pA = a.position;
        Vec3& pB = b.position;
        Vec3& vA = a.velocity;
        Vec3& vB = b.velocity;

        const Vec3 d = pB - pA;
        const double dist2 = d.dot(d);
        if (!std::isfinite(dist2) || dist2 < 0.0) {
            return stats;
        }
        const double dist = std::sqrt(dist2);

        const double pen = (a.radius + b.radius) - dist;
        if (!std::isfinite(pen)) {
            return stats;
        }

        Vec3 n{};
        if (!contactNormal(a, b, n)) {
            return stats;
        }
        stats.normal = n;
        stats.hasNormal = true;

        const double invMassSum = wA + wB;
        if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
            return stats;
        }

        const double correction =
            std::max(0.0, pen - params.penetrationSlop) * params.positionCorrectionPercent;
        pA = pA - n * (correction * wA / invMassSum);
        pB = pB + n * (correction * wB / invMassSum);

        const double vN = (vB - vA).dot(n);
        if (vN >= 0.0) {
            return stats;
        }

        const double impulse = -(1 + params.restitution) * vN / invMassSum;
        applyNormalImpulse(a, b, n, impulse);
        stats.normalImpulse = impulse;
        stats.impulseApplied = true;
        return stats;
    }
} // namespace sim::collision

#include "Collision.h"
#include <algorithm>
#include <cmath>

namespace sim::collision {
    namespace {
        [[nodiscard]] double epsilon(const Body& a, const Body& b) {
            return std::max((a.radius + b.radius) * 1e-6, 1e-12);
        }

        [[nodiscard]] double invInertiaSphere(const Body& b) {
            if (std::isfinite(b.invInertia) && b.invInertia > 0.0) {
                return b.invInertia;
            }
            if (!std::isfinite(b.invMass) || b.invMass <= 0.0 || !std::isfinite(b.radius) || b.radius <= 0.0) {
                return 0.0;
            }
            // Solid sphere: I = 2/5 m r^2 => invI = 5/2 * invMass / r^2
            return 2.5 * b.invMass / (b.radius * b.radius);
        }

        [[nodiscard]] Vec3 contactOffsetA(const Body& a, const Vec3& normal) {
            return normal * a.radius;
        }

        [[nodiscard]] Vec3 contactOffsetB(const Body& b, const Vec3& normal) {
            return normal * (-b.radius);
        }

        [[nodiscard]] Vec3 contactRelativeVelocity(const Body& a, const Body& b, const Vec3& rA, const Vec3& rB) {
            const Vec3 vAContact = a.velocity + a.angularVelocity.cross(rA);
            const Vec3 vBContact = b.velocity + b.angularVelocity.cross(rB);
            return vBContact - vAContact;
        }

        [[nodiscard]] double effectiveMassAlong(
            const Body& a, const Body& b,
            const Vec3& rA, const Vec3& rB, const Vec3& dir)
        {
            const double invIA = invInertiaSphere(a);
            const double invIB = invInertiaSphere(b);
            const Vec3 rAxDir = rA.cross(dir);
            const Vec3 rBxDir = rB.cross(dir);
            const double rotA = invIA * rAxDir.dot(rAxDir);
            const double rotB = invIB * rBxDir.dot(rBxDir);
            return a.invMass + b.invMass + rotA + rotB;
        }

        void applyImpulseAtContact(Body& a, Body& b, const Vec3& rA, const Vec3& rB, const Vec3& impulse) {
            if (!std::isfinite(impulse.x) || !std::isfinite(impulse.y) || !std::isfinite(impulse.z)) {
                return;
            }

            const double wA = a.invMass;
            const double wB = b.invMass;
            if (!std::isfinite(wA) || !std::isfinite(wB) || (wA == 0.0 && wB == 0.0)) {
                return;
            }

            a.velocity -= impulse * wA;
            b.velocity += impulse * wB;

            const double invIA = invInertiaSphere(a);
            const double invIB = invInertiaSphere(b);
            if (invIA > 0.0) {
                a.angularVelocity -= (rA.cross(impulse)) * invIA;
            }
            if (invIB > 0.0) {
                b.angularVelocity += (rB.cross(impulse)) * invIB;
            }
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

        const Vec3 rA = contactOffsetA(a, normal);
        const Vec3 rB = contactOffsetB(b, normal);
        applyImpulseAtContact(a, b, rA, rB, normal * impulse);
    }

    void applyTangentImpulse(Body& a, Body& b, const Vec3& normal, const Vec3& tangent, const double impulse)
    {
        if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z) ||
            !std::isfinite(impulse) || impulse == 0.0) {
            return;
        }
        const Vec3 rA = contactOffsetA(a, normal);
        const Vec3 rB = contactOffsetB(b, normal);
        applyImpulseAtContact(a, b, rA, rB, tangent * impulse);
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

        const Vec3 rA = contactOffsetA(a, n);
        const Vec3 rB = contactOffsetB(b, n);

        const double effMassN = effectiveMassAlong(a, b, rA, rB, n);
        if (!std::isfinite(effMassN) || effMassN <= 0.0) {
            return stats;
        }

        const double invMassSum = wA + wB;
        if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
            return stats;
        }

        const double correction =
            std::max(0.0, pen - params.penetrationSlop) * params.positionCorrectionPercent;
        pA -= n * (correction * wA / invMassSum);
        pB += n * (correction * wB / invMassSum);

        const Vec3 rv = contactRelativeVelocity(a, b, rA, rB);
        const double vN = rv.dot(n);
        if (vN >= 0.0) {
            return stats;
        }

        const double normalImpulse = -(1 + params.restitution) * vN / effMassN;
        applyNormalImpulse(a, b, n, normalImpulse);
        stats.normalImpulse = normalImpulse;
        stats.impulseApplied = true;

        // Coulomb friction: solve tangent impulse after normal impulse update.
        const Vec3 rv2 = contactRelativeVelocity(a, b, rA, rB);
        const Vec3 tangentUnscaled = rv2 - n * rv2.dot(n);
        const double tangentLen = tangentUnscaled.magnitude();
        if (tangentLen > 1e-12) {
            const Vec3 t = tangentUnscaled / tangentLen;
            const double effMassT = effectiveMassAlong(a, b, rA, rB, t);
            if (effMassT <= 0.0 || !std::isfinite(effMassT)) {
                return stats;
            }
            const double jt = -rv2.dot(t) / effMassT;

            double tangentImpulse = 0.0;
            const double maxStatic = normalImpulse * std::max(0.0, params.staticFriction);
            if (std::abs(jt) <= maxStatic) {
                tangentImpulse = jt;
            } else {
                tangentImpulse = -normalImpulse * std::max(0.0, params.dynamicFriction) * std::copysign(1.0, jt);
            }

            applyTangentImpulse(a, b, n, t, tangentImpulse);
            if (std::abs(tangentImpulse) > 0.0) {
                stats.impulseApplied = true;
            }
        }

        return stats;
    }
} // namespace sim::collision

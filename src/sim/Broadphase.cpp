#include "Broadphase.h"
#include <algorithm>
#include <cmath>

namespace sim::broadphase {
    namespace {
        struct AxisInterval {
            std::size_t idx = 0;
            double minX = 0.0;
            double maxX = 0.0;
            double minY = 0.0;
            double maxY = 0.0;
            double minZ = 0.0;
            double maxZ = 0.0;
        };

        [[nodiscard]] bool overlapsYZ(const AxisInterval& a, const AxisInterval& b) {
            return a.maxY >= b.minY && b.maxY >= a.minY &&
                   a.maxZ >= b.minZ && b.maxZ >= a.minZ;
        }

        template <typename Builder>
        std::vector<Pair> sapPairs(const std::vector<Body>& bodies, Builder&& builder)
        {
            std::vector<Pair> pairs;
            if (bodies.size() < 2) {
                return pairs;
            }

            thread_local std::vector<AxisInterval> intervals;
            intervals.clear();
            intervals.reserve(bodies.size());

            for (std::size_t i = 0; i < bodies.size(); ++i) {
                AxisInterval in;
                if (builder(bodies[i], i, in)) {
                    intervals.push_back(in);
                }
            }

            if (intervals.size() < 2) {
                return pairs;
            }

            std::ranges::sort(intervals, [](const AxisInterval& a, const AxisInterval& b) {
                return a.minX < b.minX;
            });

            for (std::size_t i = 0; i + 1 < intervals.size(); ++i) {
                const AxisInterval& a = intervals[i];
                for (std::size_t j = i + 1; j < intervals.size(); ++j) {
                    const AxisInterval& b = intervals[j];
                    if (b.minX > a.maxX) {
                        break;
                    }
                    if (overlapsYZ(a, b)) {
                        pairs.emplace_back(std::min(a.idx, b.idx), std::max(a.idx, b.idx));
                    }
                }
            }

            return pairs;
        }

        [[nodiscard]] bool buildDiscreteInterval(const Body& b, const std::size_t index, AxisInterval& out)
        {
            if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z) ||
                !std::isfinite(b.radius) || b.radius < 0.0) {
                return false;
            }

            out.idx = index;
            out.minX = b.position.x - b.radius;
            out.maxX = b.position.x + b.radius;
            out.minY = b.position.y - b.radius;
            out.maxY = b.position.y + b.radius;
            out.minZ = b.position.z - b.radius;
            out.maxZ = b.position.z + b.radius;
            return std::isfinite(out.minX) && std::isfinite(out.maxX) &&
                   std::isfinite(out.minY) && std::isfinite(out.maxY) &&
                   std::isfinite(out.minZ) && std::isfinite(out.maxZ);
        }

        [[nodiscard]] bool buildSweptInterval(
            const Body& b,
            const std::size_t index,
            const double maxTime,
            AxisInterval& out)
        {
            if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z) ||
                !std::isfinite(b.velocity.x) || !std::isfinite(b.velocity.y) || !std::isfinite(b.velocity.z) ||
                !std::isfinite(b.radius) || b.radius < 0.0) {
                return false;
            }

            const Vec3 endPos = b.position + b.velocity * std::max(0.0, maxTime);
            if (!std::isfinite(endPos.x) || !std::isfinite(endPos.y) || !std::isfinite(endPos.z)) {
                return false;
            }

            out.idx = index;
            out.minX = std::min(b.position.x, endPos.x) - b.radius;
            out.maxX = std::max(b.position.x, endPos.x) + b.radius;
            out.minY = std::min(b.position.y, endPos.y) - b.radius;
            out.maxY = std::max(b.position.y, endPos.y) + b.radius;
            out.minZ = std::min(b.position.z, endPos.z) - b.radius;
            out.maxZ = std::max(b.position.z, endPos.z) + b.radius;
            return std::isfinite(out.minX) && std::isfinite(out.maxX) &&
                   std::isfinite(out.minY) && std::isfinite(out.maxY) &&
                   std::isfinite(out.minZ) && std::isfinite(out.maxZ);
        }
    } // namespace

    std::vector<Pair> discretePairs(const std::vector<Body>& bodies)
    {
        return sapPairs(bodies, buildDiscreteInterval);
    }

    std::vector<Pair> sweptPairs(const std::vector<Body>& bodies, const double maxTime)
    {
        return sapPairs(bodies, [maxTime](const Body& b, const std::size_t index, AxisInterval& out) {
            return buildSweptInterval(b, index, maxTime, out);
        });
    }
} // namespace sim::broadphase

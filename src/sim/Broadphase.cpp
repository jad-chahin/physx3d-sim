//
// Broadphase pair generation helpers.
//

#include "Broadphase.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace sim::broadphase {
    namespace {
        struct PairHash {
            std::size_t operator()(const Pair& p) const {
                std::size_t h = 0xcbf29ce484222325ULL;
                h ^= std::hash<std::size_t>{}(p.first) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                h ^= std::hash<std::size_t>{}(p.second) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                return h;
            }
        };

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

        std::vector<Pair> discreteSapPairs(const std::vector<Body>& bodies)
        {
            std::vector<Pair> pairs;
            if (bodies.size() < 2) {
                return pairs;
            }

            thread_local std::vector<AxisInterval> intervals;
            thread_local std::vector<std::size_t> active;
            intervals.clear();
            active.clear();
            intervals.reserve(bodies.size());
            active.reserve(bodies.size());

            for (std::size_t i = 0; i < bodies.size(); ++i) {
                const Body& b = bodies[i];
                if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z) ||
                    !std::isfinite(b.radius) || b.radius < 0.0) {
                    continue;
                }

                AxisInterval in;
                in.idx = i;
                in.minX = b.position.x - b.radius;
                in.maxX = b.position.x + b.radius;
                in.minY = b.position.y - b.radius;
                in.maxY = b.position.y + b.radius;
                in.minZ = b.position.z - b.radius;
                in.maxZ = b.position.z + b.radius;
                if (!std::isfinite(in.minX) || !std::isfinite(in.maxX) ||
                    !std::isfinite(in.minY) || !std::isfinite(in.maxY) ||
                    !std::isfinite(in.minZ) || !std::isfinite(in.maxZ)) {
                    continue;
                }
                intervals.push_back(in);
            }

            std::ranges::sort(intervals, [](const AxisInterval& a, const AxisInterval& b) {
                return a.minX < b.minX;
            });

            for (std::size_t cur = 0; cur < intervals.size(); ++cur) {
                const AxisInterval& c = intervals[cur];
                active.erase(std::ranges::remove_if(active, [&](const std::size_t idx) {
                    return intervals[idx].maxX < c.minX;
                }).begin(), active.end());

                for (const std::size_t idx : active) {
                    if (overlapsYZ(c, intervals[idx])) {
                        pairs.emplace_back(std::min(c.idx, intervals[idx].idx), std::max(c.idx, intervals[idx].idx));
                    }
                }
                active.push_back(cur);
            }

            return pairs;
        }

        std::vector<Pair> sweptSapPairs(const std::vector<Body>& bodies, const double maxTime)
        {
            std::vector<Pair> pairs;
            if (bodies.size() < 2) {
                return pairs;
            }

            const double clampedTime = std::max(0.0, maxTime);
            thread_local std::vector<AxisInterval> intervals;
            thread_local std::vector<std::size_t> active;
            thread_local std::unordered_set<Pair, PairHash> seen;
            intervals.clear();
            active.clear();
            seen.clear();
            intervals.reserve(bodies.size());
            active.reserve(bodies.size());
            seen.reserve(bodies.size() * 4);

            for (std::size_t i = 0; i < bodies.size(); ++i) {
                const Body& b = bodies[i];
                if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z) ||
                    !std::isfinite(b.velocity.x) || !std::isfinite(b.velocity.y) || !std::isfinite(b.velocity.z) ||
                    !std::isfinite(b.radius) || b.radius < 0.0) {
                    continue;
                }
                const Vec3 endPos = b.position + b.velocity * clampedTime;
                if (!std::isfinite(endPos.x) || !std::isfinite(endPos.y) || !std::isfinite(endPos.z)) {
                    continue;
                }

                AxisInterval in;
                in.idx = i;
                in.minX = std::min(b.position.x, endPos.x) - b.radius;
                in.maxX = std::max(b.position.x, endPos.x) + b.radius;
                in.minY = std::min(b.position.y, endPos.y) - b.radius;
                in.maxY = std::max(b.position.y, endPos.y) + b.radius;
                in.minZ = std::min(b.position.z, endPos.z) - b.radius;
                in.maxZ = std::max(b.position.z, endPos.z) + b.radius;
                if (!std::isfinite(in.minX) || !std::isfinite(in.maxX) ||
                    !std::isfinite(in.minY) || !std::isfinite(in.maxY) ||
                    !std::isfinite(in.minZ) || !std::isfinite(in.maxZ)) {
                    continue;
                }
                intervals.push_back(in);
            }

            std::ranges::sort(intervals, [](const AxisInterval& a, const AxisInterval& b) {
                return a.minX < b.minX;
            });

            for (std::size_t cur = 0; cur < intervals.size(); ++cur) {
                const AxisInterval& c = intervals[cur];
                active.erase(std::ranges::remove_if(active, [&](const std::size_t idx) {
                    return intervals[idx].maxX < c.minX;
                }).begin(), active.end());

                for (const std::size_t idx : active) {
                    if (overlapsYZ(c, intervals[idx])) {
                        const Pair key{std::min(c.idx, intervals[idx].idx), std::max(c.idx, intervals[idx].idx)};
                        if (seen.insert(key).second) {
                            pairs.push_back(key);
                        }
                    }
                }
                active.push_back(cur);
            }

            return pairs;
        }
    } // namespace

    std::vector<Pair> discretePairs(const std::vector<Body>& bodies)
    {
        return discreteSapPairs(bodies);
    }

    std::vector<Pair> sweptPairs(const std::vector<Body>& bodies, const double maxTime)
    {
        return sweptSapPairs(bodies, maxTime);
    }
} // namespace sim::broadphase

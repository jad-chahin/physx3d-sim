//
// Broadphase pair generation helpers.
//

#include "Broadphase.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace sim::broadphase {
    namespace {
        struct GridKey {
            std::int64_t x = 0;
            std::int64_t y = 0;
            std::int64_t z = 0;

            bool operator==(const GridKey& o) const {
                return x == o.x && y == o.y && z == o.z;
            }
        };

        struct GridKeyHash {
            std::size_t operator()(const GridKey& k) const {
                std::size_t h = 0xcbf29ce484222325ULL;
                const auto mix = [&h](const std::size_t v) {
                    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                };
                mix(std::hash<std::int64_t>{}(k.x));
                mix(std::hash<std::int64_t>{}(k.y));
                mix(std::hash<std::int64_t>{}(k.z));
                return h;
            }
        };

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

        void appendPairsFromGridCells(
            std::unordered_map<GridKey, std::vector<std::size_t>, GridKeyHash>& grid,
            std::unordered_set<Pair, PairHash>& seen,
            std::vector<Pair>& outPairs)
        {
            for (const auto& [cell, ids] : grid) {
                (void)cell;
                const std::size_t count = ids.size();
                for (std::size_t a = 0; a < count; ++a) {
                    for (std::size_t b = a + 1; b < count; ++b) {
                        const Pair key{std::min(ids[a], ids[b]), std::max(ids[a], ids[b])};
                        if (key.first == key.second) {
                            continue;
                        }
                        if (seen.insert(key).second) {
                            outPairs.push_back(key);
                        }
                    }
                }
            }
        }

        [[nodiscard]] double cellSize(const std::vector<Body>& bodies)
        {
            std::vector<double> radii;
            radii.reserve(bodies.size());
            for (const Body& b : bodies) {
                if (std::isfinite(b.radius) && b.radius > 0.0) {
                    radii.push_back(b.radius);
                }
            }
            if (radii.empty()) {
                return 1e-6;
            }

            const std::size_t mid = radii.size() / 2;
            std::nth_element(radii.begin(), radii.begin() + static_cast<std::ptrdiff_t>(mid), radii.end());
            return std::max(2.0 * radii[mid], 1e-6);
        }

        [[nodiscard]] bool useSap(const std::vector<Body>& bodies)
        {
            (void)bodies;
            return true;
        }

        std::vector<Pair> discreteGridPairs(const std::vector<Body>& bodies)
        {
            std::vector<Pair> pairs;
            if (bodies.size() < 2) {
                return pairs;
            }

            const double cs = cellSize(bodies);
            thread_local std::unordered_map<GridKey, std::vector<std::size_t>, GridKeyHash> grid;
            thread_local std::unordered_set<Pair, PairHash> seen;
            grid.clear();
            seen.clear();
            grid.reserve(bodies.size() * 2);
            seen.reserve(bodies.size() * 4);

            for (std::size_t i = 0; i < bodies.size(); ++i) {
                const Body& b = bodies[i];
                if (!std::isfinite(b.position.x) || !std::isfinite(b.position.y) || !std::isfinite(b.position.z) ||
                    !std::isfinite(b.radius) || b.radius < 0.0) {
                    continue;
                }

                const double minX = (b.position.x - b.radius) / cs;
                const double minY = (b.position.y - b.radius) / cs;
                const double minZ = (b.position.z - b.radius) / cs;
                const double maxX = (b.position.x + b.radius) / cs;
                const double maxY = (b.position.y + b.radius) / cs;
                const double maxZ = (b.position.z + b.radius) / cs;
                if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(minZ) ||
                    !std::isfinite(maxX) || !std::isfinite(maxY) || !std::isfinite(maxZ)) {
                    continue;
                }

                const std::int64_t x0 = static_cast<std::int64_t>(std::floor(minX));
                const std::int64_t y0 = static_cast<std::int64_t>(std::floor(minY));
                const std::int64_t z0 = static_cast<std::int64_t>(std::floor(minZ));
                const std::int64_t x1 = static_cast<std::int64_t>(std::floor(maxX));
                const std::int64_t y1 = static_cast<std::int64_t>(std::floor(maxY));
                const std::int64_t z1 = static_cast<std::int64_t>(std::floor(maxZ));

                for (std::int64_t x = x0; x <= x1; ++x) {
                    for (std::int64_t y = y0; y <= y1; ++y) {
                        for (std::int64_t z = z0; z <= z1; ++z) {
                            grid[{x, y, z}].push_back(i);
                        }
                    }
                }
            }

            appendPairsFromGridCells(grid, seen, pairs);
            return pairs;
        }

        std::vector<Pair> sweptGridPairs(const std::vector<Body>& bodies, const double maxTime)
        {
            std::vector<Pair> pairs;
            if (bodies.size() < 2) {
                return pairs;
            }

            const double clampedTime = std::max(0.0, maxTime);
            const double cs = cellSize(bodies);
            thread_local std::unordered_map<GridKey, std::vector<std::size_t>, GridKeyHash> grid;
            thread_local std::unordered_set<Pair, PairHash> seen;
            grid.clear();
            seen.clear();
            grid.reserve(bodies.size() * 2);
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

                const double minX = (std::min(b.position.x, endPos.x) - b.radius) / cs;
                const double minY = (std::min(b.position.y, endPos.y) - b.radius) / cs;
                const double minZ = (std::min(b.position.z, endPos.z) - b.radius) / cs;
                const double maxX = (std::max(b.position.x, endPos.x) + b.radius) / cs;
                const double maxY = (std::max(b.position.y, endPos.y) + b.radius) / cs;
                const double maxZ = (std::max(b.position.z, endPos.z) + b.radius) / cs;
                if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(minZ) ||
                    !std::isfinite(maxX) || !std::isfinite(maxY) || !std::isfinite(maxZ)) {
                    continue;
                }

                const std::int64_t x0 = static_cast<std::int64_t>(std::floor(minX));
                const std::int64_t y0 = static_cast<std::int64_t>(std::floor(minY));
                const std::int64_t z0 = static_cast<std::int64_t>(std::floor(minZ));
                const std::int64_t x1 = static_cast<std::int64_t>(std::floor(maxX));
                const std::int64_t y1 = static_cast<std::int64_t>(std::floor(maxY));
                const std::int64_t z1 = static_cast<std::int64_t>(std::floor(maxZ));

                for (std::int64_t x = x0; x <= x1; ++x) {
                    for (std::int64_t y = y0; y <= y1; ++y) {
                        for (std::int64_t z = z0; z <= z1; ++z) {
                            grid[{x, y, z}].push_back(i);
                        }
                    }
                }
            }

            appendPairsFromGridCells(grid, seen, pairs);
            return pairs;
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

            std::sort(intervals.begin(), intervals.end(), [](const AxisInterval& a, const AxisInterval& b) {
                return a.minX < b.minX;
            });

            for (std::size_t cur = 0; cur < intervals.size(); ++cur) {
                const AxisInterval& c = intervals[cur];
                active.erase(std::remove_if(active.begin(), active.end(), [&](const std::size_t idx) {
                    return intervals[idx].maxX < c.minX;
                }), active.end());

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

            std::sort(intervals.begin(), intervals.end(), [](const AxisInterval& a, const AxisInterval& b) {
                return a.minX < b.minX;
            });

            for (std::size_t cur = 0; cur < intervals.size(); ++cur) {
                const AxisInterval& c = intervals[cur];
                active.erase(std::remove_if(active.begin(), active.end(), [&](const std::size_t idx) {
                    return intervals[idx].maxX < c.minX;
                }), active.end());

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
        return useSap(bodies) ? discreteSapPairs(bodies) : discreteGridPairs(bodies);
    }

    std::vector<Pair> sweptPairs(const std::vector<Body>& bodies, const double maxTime)
    {
        return useSap(bodies) ? sweptSapPairs(bodies, maxTime) : sweptGridPairs(bodies, maxTime);
    }
} // namespace sim::broadphase

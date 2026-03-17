#ifndef PHYSICS3D_BROADPHASE_H
#define PHYSICS3D_BROADPHASE_H

#include <cstddef>
#include <utility>
#include <vector>
#include "Body.h"

namespace sim::broadphase {
    using Pair = std::pair<std::size_t, std::size_t>;

    void discretePairs(const std::vector<Body>& bodies, std::vector<Pair>& outPairs);
    [[nodiscard]] std::vector<Pair> discretePairs(const std::vector<Body>& bodies);
    void sweptPairs(const std::vector<Body>& bodies, double maxTime, std::vector<Pair>& outPairs);
    [[nodiscard]] std::vector<Pair> sweptPairs(const std::vector<Body>& bodies, double maxTime);
} // namespace sim::broadphase

#endif // PHYSICS3D_BROADPHASE_H

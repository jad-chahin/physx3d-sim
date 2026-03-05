//
// Broadphase pair generation helpers.
//

#ifndef PHYSICS3D_BROADPHASE_H
#define PHYSICS3D_BROADPHASE_H

#include <cstddef>
#include <utility>
#include <vector>
#include "Body.h"

namespace sim::broadphase {
    using Pair = std::pair<std::size_t, std::size_t>;

    [[nodiscard]] std::vector<Pair> discretePairs(const std::vector<Body>& bodies);
    [[nodiscard]] std::vector<Pair> sweptPairs(const std::vector<Body>& bodies, double maxTime);
} // namespace sim::broadphase

#endif // PHYSICS3D_BROADPHASE_H

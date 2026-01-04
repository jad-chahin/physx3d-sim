//
// Created by jchah on 2026-01-04.
//

#include "render/SphereMesh.h"
#include <algorithm>
#include <glm/glm.hpp>
#include "glm/ext/scalar_constants.hpp"
#include "glm/gtc/constants.hpp"


void buildSphereMesh(
    int stacks, int slices,
    std::vector<float>& outVerts,
    std::vector<std::uint32_t>& outIdx)
{
    stacks = std::max(2, stacks);
    slices = std::max(3, slices);

    outVerts.clear();
    outIdx.clear();

    for (int i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = glm::pi<float>() * v;

        const float y = std::cos(phi);
        const float r = std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(slices);
            const float theta = glm::two_pi<float>() * u;

            const float x = r * std::cos(theta);
            const float z = r * std::sin(theta);

            outVerts.push_back(x);
            outVerts.push_back(y);
            outVerts.push_back(z);

            outVerts.push_back(x);
            outVerts.push_back(y);
            outVerts.push_back(z);
        }
    }

    const int stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const auto i0 = static_cast<std::uint32_t>(i * stride + j);
            const auto i1 = static_cast<std::uint32_t>((i + 1) * stride + j);
            const auto i2 = static_cast<std::uint32_t>((i + 1) * stride + (j + 1));
            const auto i3 = static_cast<std::uint32_t>(i * stride + (j + 1));

            outIdx.push_back(i0); outIdx.push_back(i1); outIdx.push_back(i2);
            outIdx.push_back(i0); outIdx.push_back(i2); outIdx.push_back(i3);
        }
    }
}

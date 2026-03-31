#ifndef PHYSICS3D_RENDER_FRAMERENDERERINTERNAL_H
#define PHYSICS3D_RENDER_FRAMERENDERERINTERNAL_H

#include "render/FrameRenderer.h"

#include "render/SceneLighting.h"
#include "render/ShaderUtil.h"
#include "render/SphereMesh.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace render_scene {

inline bool sameVec2(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

} // namespace render_scene

#endif // PHYSICS3D_RENDER_FRAMERENDERERINTERNAL_H

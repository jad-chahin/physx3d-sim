//
// Created by jchah on 2026-01-04.
//

#include "input/Camera.h"
#include <algorithm>
#include <cmath>


void clampPitch(Camera& c) {
    constexpr float lim = glm::radians(89.0f);
    c.pitch = std::clamp(c.pitch, -lim, lim);
}

glm::vec3 forwardDir(const Camera& c) {
    const float cy = std::cos(c.yaw);
    const float sy = std::sin(c.yaw);
    const float cp = std::cos(c.pitch);
    const float sp = std::sin(c.pitch);
    return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

glm::vec3 rightDir(const Camera& c) {
    return glm::normalize(glm::cross(forwardDir(c), glm::vec3(0.0f, 1.0f, 0.0f)));
}

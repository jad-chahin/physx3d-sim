#include "input/Camera.h"
#include <algorithm>
#include <cmath>

namespace {
    glm::vec3 computeForwardDir(const input::Camera& c) {
        const float cy = std::cos(c.yaw);
        const float sy = std::sin(c.yaw);
        const float cp = std::cos(c.pitch);
        const float sp = std::sin(c.pitch);
        return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
    }
}

namespace input {
    void clampPitch(Camera& c) {
        constexpr float lim = glm::radians(89.0f);
        c.pitch = std::clamp(c.pitch, -lim, lim);
    }

    glm::vec3 forwardDir(const Camera& c) {
        return computeForwardDir(c);
    }

    glm::vec3 rightDir(const Camera& c) {
        return glm::normalize(glm::cross(computeForwardDir(c), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void setLookDirection(Camera& c, const glm::vec3& direction) {
        const float lenSq = glm::dot(direction, direction);
        if (lenSq <= 1e-12f) {
            return;
        }

        const glm::vec3 normalized = glm::normalize(direction);
        const float horizontalLenSq = normalized.x * normalized.x + normalized.z * normalized.z;
        if (horizontalLenSq > 1e-8f) {
            c.yaw = std::atan2(normalized.z, normalized.x);
        }
        c.pitch = std::asin(std::clamp(normalized.y, -1.0f, 1.0f));
        clampPitch(c);
    }
}

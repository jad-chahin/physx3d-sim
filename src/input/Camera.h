//
// Created by jchah on 2026-01-04.
//


#ifndef PHYSICS3D_CAMERA_H
#define PHYSICS3D_CAMERA_H

#include <glm/glm.hpp>
#include "glm/gtc/constants.hpp"


struct Camera {
    glm::vec3 pos{0.0f, 0.0f, 30.0f};
    float yaw   = -glm::half_pi<float>(); // looking toward -Z by default
    float pitch = 0.0f;

    float moveSpeed = 20.0f; // units/sec
    float lookSpeed = 0.0025f;
};

void clampPitch(Camera& c);
glm::vec3 forwardDir(const Camera& c);
glm::vec3 rightDir(const Camera& c);

#endif //PHYSICS3D_CAMERA_H
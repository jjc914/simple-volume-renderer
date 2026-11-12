#ifndef CAMERA_ORBIT_CAMERA_HPP
#define CAMERA_ORBIT_CAMERA_HPP

#include "camera/camera.hpp"

// Orbits the origin; left-drag rotates, scroll zooms.
class OrbitCamera : public Camera {
public:
    glm::vec3 Position() const override;
    glm::mat4 View() const override;

    void OnMouseDrag(float dx, float dy) override;
    void OnScroll(float amount) override;

    float yaw = 0.6f;
    float pitch = 0.4f;
    float distance = 2.5f;
};

#endif

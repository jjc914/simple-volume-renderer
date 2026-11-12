#include "camera/orbit_camera.hpp"

#include <algorithm>
#include <cmath>

glm::vec3 OrbitCamera::Position() const {
    return distance * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                std::sin(pitch),
                                std::cos(pitch) * std::cos(yaw));
}

glm::mat4 OrbitCamera::View() const {
    return glm::lookAt(Position(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::OnMouseDrag(float dx, float dy) {
    yaw -= dx * 0.01f;
    pitch = std::clamp(pitch + dy * 0.01f, -1.5f, 1.5f);
}

void OrbitCamera::OnScroll(float amount) {
    distance = std::clamp(distance * std::pow(0.9f, amount), 0.5f, 10.0f);
}

#ifndef CAMERA_CAMERA_HPP
#define CAMERA_CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Snapshot of a camera for one frame; this is what renderers consume.
struct CameraState {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};

    glm::mat4 ViewProjection() const { return projection * view; }
};

// Camera interface. Subclasses define the pose (Position/View) and how input
// moves it; the default projection is a perspective one, override
// Projection() for anything else.
class Camera {
public:
    virtual ~Camera() = default;

    virtual glm::vec3 Position() const = 0;
    virtual glm::mat4 View() const = 0;
    virtual glm::mat4 Projection(float aspect) const {
        return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
    }

    // Input hooks. Defaults ignore input. dx/dy are in pixels.
    virtual void OnMouseDrag(float dx, float dy) {}
    virtual void OnScroll(float amount) {}

    CameraState State(float aspect) const {
        CameraState s;
        s.position = Position();
        s.view = View();
        s.projection = Projection(aspect);
        return s;
    }

    float fovDegrees = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

#endif

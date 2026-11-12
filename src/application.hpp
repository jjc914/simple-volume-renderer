#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "camera/camera.hpp"
#include "renderers/renderer.hpp"

#include <memory>

struct GLFWwindow;

// Owns the window, GL context, ImGui, camera/input and the renderer, and
// runs the main loop.
class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Create the window/context, ImGui, camera and renderer.
    // Returns false on failure (errors are printed to stderr).
    bool Init(int width, int height, const char* title);

    // Run until the window is closed.
    void Run();

private:
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void UpdateInput();
    void DrawUI();

    // Declaration order matters: renderer_ is destroyed before the context.
    GLFWwindow* window_ = nullptr;
    bool imguiReady_ = false;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Camera> camera_;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

#endif

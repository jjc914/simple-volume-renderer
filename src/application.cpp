#include "application.hpp"

#include "camera/orbit_camera.hpp"
#include "renderers/grid_volume_renderer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>


Application::~Application() {
    // The renderer releases GL objects, so it must go before the context.
    if (renderer_) renderer_->Shutdown();
    renderer_.reset();

    if (imguiReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Application::Init(int width, int height, const char* title) {
    camera_ = std::make_unique<OrbitCamera>();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Set our callback before ImGui installs its own so ImGui chains to it.
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, ScrollCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    imguiReady_ = true;

    auto renderer = std::make_unique<GridVolumeRenderer>();
    if (renderer->AddVolumesFromDirectory("resources/volumes") == 0) {
        std::cerr << "No volumes found in resources/volumes" << std::endl;
    }
    if (!renderer->Init()) {
        std::cerr << "Failed to initialize " << renderer->Name() << std::endl;
        return false;
    }
    renderer_ = std::move(renderer);
    return true;
}

void Application::ScrollCallback(GLFWwindow* window, double, double yoffset) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->camera_->OnScroll(float(yoffset));
}

void Application::UpdateInput() {
    // Orbit with left-mouse drag (unless ImGui wants the mouse).
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    if (!ImGui::GetIO().WantCaptureMouse &&
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        camera_->OnMouseDrag(float(mx - lastMouseX_), float(my - lastMouseY_));
    }
    lastMouseX_ = mx;
    lastMouseY_ = my;
}

void Application::DrawUI() {
    ImGui::Begin("Simple Volume Renderer");
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    if (renderer_) {
        ImGui::Separator();
        ImGui::TextUnformatted(renderer_->Name());
        renderer_->DrawUI();
    }
    ImGui::End();
}

void Application::Run() {
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        UpdateInput();

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) continue;   // minimized

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        DrawUI();
        ImGui::Render();

        FrameContext ctx;
        ctx.camera = camera_->State(float(fbWidth) / float(fbHeight));
        ctx.target.framebuffer = 0;
        ctx.target.width = fbWidth;
        ctx.target.height = fbHeight;
        ctx.time = float(now);
        ctx.deltaTime = dt;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (renderer_) renderer_->Render(ctx);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

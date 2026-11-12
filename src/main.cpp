#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <iostream>
#include <array>
#include <cmath>
#include <vector>

#include "file.hpp"
#include "mesh.hpp"
#include "renderer.hpp"
#include "grid_volume_renderer.hpp"


// Orbit camera state, driven by mouse drag / scroll.
struct OrbitCamera {
    float yaw = 0.6f;
    float pitch = 0.4f;
    float distance = 2.5f;

    glm::vec3 Position() const {
        return distance * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                    std::sin(pitch),
                                    std::cos(pitch) * std::cos(yaw));
    }
};

static OrbitCamera g_orbit;

static void ScrollCallback(GLFWwindow*, double, double yoffset) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
    g_orbit.distance = std::clamp(g_orbit.distance * std::pow(0.9f, float(yoffset)), 0.5f, 10.0f);
}

// Procedural test volume: a hollow sphere shell with a wavy surface plus a
// dense core, values in [0, 1].
static std::vector<float> MakeTestVolume(int n) {
    std::vector<float> voxels(size_t(n) * n * n);
    for (int z = 0; z < n; ++z)
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                glm::vec3 p = (glm::vec3(x, y, z) + 0.5f) / float(n) * 2.0f - 1.0f;
                float r = glm::length(p);
                float wave = 0.04f * std::sin(10.0f * p.x) * std::sin(10.0f * p.y) * std::sin(10.0f * p.z);
                float shell = std::exp(-std::pow((r - 0.7f + wave) / 0.08f, 2.0f));
                float core = std::exp(-std::pow(r / 0.25f, 2.0f));
                voxels[(size_t(z) * n + y) * n + x] = std::clamp(0.8f * shell + 0.5f * core, 0.0f, 1.0f);
            }
    return voxels;
}

int main() {
    unsigned int width = 1400;
    unsigned int height = 800;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(width, height, "Simple Volume Renderer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Set our callback before ImGui installs its own so ImGui chains to it.
    glfwSetScrollCallback(window, ScrollCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int exitCode = 0;
    {   // Scope so the renderer releases GL objects before the context dies.
        GridVolumeRenderer volumeRenderer;
        if (!volumeRenderer.Init()) {
            std::cerr << "Failed to initialize " << volumeRenderer.Name() << std::endl;
            exitCode = -1;
        } else {
            const int N = 96;
            volumeRenderer.SetVolume(MakeTestVolume(N), N, N, N);
        }

        double lastTime = glfwGetTime();
        double lastX = 0.0, lastY = 0.0;

        while (exitCode == 0 && !glfwWindowShouldClose(window)) {
            glfwPollEvents();

            double now = glfwGetTime();
            float dt = float(now - lastTime);
            lastTime = now;

            // Orbit with left-mouse drag (unless ImGui wants the mouse).
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            if (!ImGui::GetIO().WantCaptureMouse &&
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                g_orbit.yaw   -= float(mx - lastX) * 0.01f;
                g_orbit.pitch  = std::clamp(g_orbit.pitch + float(my - lastY) * 0.01f, -1.5f, 1.5f);
            }
            lastX = mx;
            lastY = my;

            int fbWidth, fbHeight;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            if (fbWidth == 0 || fbHeight == 0) continue;   // minimized

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin(volumeRenderer.Name());
            ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
            volumeRenderer.DrawUI();
            ImGui::End();
            ImGui::Render();

            FrameContext ctx;
            ctx.camera.position = g_orbit.Position();
            ctx.camera.view = glm::lookAt(ctx.camera.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            ctx.camera.projection = glm::perspective(glm::radians(45.0f), float(fbWidth) / float(fbHeight), 0.1f, 100.0f);
            ctx.target.framebuffer = 0;
            ctx.target.width = fbWidth;
            ctx.target.height = fbHeight;
            ctx.time = float(now);
            ctx.deltaTime = dt;

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            volumeRenderer.Render(ctx);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}

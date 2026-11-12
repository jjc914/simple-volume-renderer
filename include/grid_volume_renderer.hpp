#ifndef GRID_VOLUME_RENDERER_HPP
#define GRID_VOLUME_RENDERER_HPP

#include "renderer.hpp"
#include "file.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <iostream>
#include <vector>

// Direct volume renderer for a dense scalar grid. Ray-marches a 3D texture in
// a fragment shader (no compute needed, so it works on macOS GL 4.1/3.3).
//
//   GridVolumeRenderer vr;
//   vr.Init();
//   vr.SetVolume(voxels, 128, 128, 128);   // scalars in [0, 1], x-fastest
//   vr.Render(frameContext);
class GridVolumeRenderer : public Renderer {
public:
    struct Params {
        int   maxSteps = 256;
        float densityScale = 1.0f;     // multiplies opacity from the transfer function
        float stepSize = 0.005f;       // in volume-local units (volume spans [0,1]^3)
        float opacityCutoff = 0.98f;   // early ray termination
        glm::vec3 boundsMin{-0.5f};    // world-space box the volume occupies
        glm::vec3 boundsMax{0.5f};
    };

    Params params;

    ~GridVolumeRenderer() override { Shutdown(); }

    bool Init() override {
        std::string vs = ReadFile("resources/shaders/volume/volume.vert");
        std::string fs = ReadFile("resources/shaders/volume/volume.frag");
        program_ = BuildProgram(vs.c_str(), fs.c_str());
        if (!program_) return false;

        // Attribute-less fullscreen triangle (vertices come from gl_VertexID),
        // but core profile still requires a bound VAO to draw.
        glGenVertexArrays(1, &vao_);

        SetTransferFunction(DefaultTransferFunction());
        return true;
    }

    void Shutdown() override {
        if (program_)     { glDeleteProgram(program_);        program_ = 0; }
        if (vao_)         { glDeleteVertexArrays(1, &vao_);   vao_ = 0; }
        if (volumeTex_)   { glDeleteTextures(1, &volumeTex_); volumeTex_ = 0; }
        if (transferTex_) { glDeleteTextures(1, &transferTex_); transferTex_ = 0; }
    }

    // Upload a width*height*depth scalar field, x varying fastest.
    void SetVolume(const std::vector<float>& voxels, int width, int height, int depth) {
        if (voxels.size() != size_t(width) * height * depth) {
            std::cerr << "GridVolumeRenderer::SetVolume: size mismatch" << std::endl;
            return;
        }
        if (!volumeTex_) glGenTextures(1, &volumeTex_);
        glBindTexture(GL_TEXTURE_3D, volumeTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0,
                     GL_RED, GL_FLOAT, voxels.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
        glBindTexture(GL_TEXTURE_3D, 0);
        dims_ = {width, height, depth};
    }

    // 256-entry RGBA lookup: scalar value -> color + opacity.
    void SetTransferFunction(const std::vector<glm::vec4>& table) {
        if (table.empty()) return;
        if (!transferTex_) glGenTextures(1, &transferTex_);
        glBindTexture(GL_TEXTURE_1D, transferTex_);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, int(table.size()), 0,
                     GL_RGBA, GL_FLOAT, table.data());
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_1D, 0);
    }

    void Render(const FrameContext& ctx) override {
        if (!program_ || !volumeTex_) return;

        // Save the state we touch.
        GLint prevFbo, prevProgram, prevVao, prevViewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        GLboolean wasBlend = glIsEnabled(GL_BLEND);
        GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, ctx.target.framebuffer);
        glViewport(0, 0, ctx.target.width, ctx.target.height);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   // shader outputs premultiplied alpha

        glUseProgram(program_);
        glm::mat4 invViewProj = glm::inverse(ctx.camera.ViewProjection());
        glUniformMatrix4fv(Loc("u_invViewProj"), 1, GL_FALSE, glm::value_ptr(invViewProj));
        glUniform3fv(Loc("u_cameraPos"), 1, glm::value_ptr(ctx.camera.position));
        glUniform3fv(Loc("u_boundsMin"), 1, glm::value_ptr(params.boundsMin));
        glUniform3fv(Loc("u_boundsMax"), 1, glm::value_ptr(params.boundsMax));
        glUniform1i(Loc("u_maxSteps"), params.maxSteps);
        glUniform1f(Loc("u_stepSize"), params.stepSize);
        glUniform1f(Loc("u_densityScale"), params.densityScale);
        glUniform1f(Loc("u_opacityCutoff"), params.opacityCutoff);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, volumeTex_);
        glUniform1i(Loc("u_volume"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, transferTex_);
        glUniform1i(Loc("u_transfer"), 1);

        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Restore.
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_1D, 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, 0);
        glBindVertexArray(prevVao);
        glUseProgram(prevProgram);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        if (!wasBlend) glDisable(GL_BLEND);
        if (wasDepth)  glEnable(GL_DEPTH_TEST);
    }

    void DrawUI() override {
        ImGui::SliderInt("Max steps", &params.maxSteps, 16, 1024);
        ImGui::SliderFloat("Step size", &params.stepSize, 0.0005f, 0.05f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Density", &params.densityScale, 0.0f, 10.0f);
        ImGui::SliderFloat("Opacity cutoff", &params.opacityCutoff, 0.5f, 1.0f);
    }

    const char* Name() const override { return "Grid Volume"; }

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint volumeTex_ = 0;
    GLuint transferTex_ = 0;
    std::array<int, 3> dims_{0, 0, 0};

    GLint Loc(const char* name) const { return glGetUniformLocation(program_, name); }

    // Simple blue -> white -> orange ramp with opacity rising with value.
    static std::vector<glm::vec4> DefaultTransferFunction() {
        std::vector<glm::vec4> table(256);
        for (int i = 0; i < 256; ++i) {
            float t = i / 255.0f;
            glm::vec3 lo(0.1f, 0.2f, 0.9f), mid(1.0f), hi(1.0f, 0.5f, 0.1f);
            glm::vec3 c = t < 0.5f ? glm::mix(lo, mid, t * 2.0f) : glm::mix(mid, hi, (t - 0.5f) * 2.0f);
            table[i] = glm::vec4(c, t * t);
        }
        return table;
    }

    static GLuint CompileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compile error:\n" << log << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint BuildProgram(const char* vsSrc, const char* fsSrc) {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
        if (!vs || !fs) {
            glDeleteShader(vs);
            glDeleteShader(fs);
            return 0;
        }
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::cerr << "Program link error:\n" << log << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }
};

#endif

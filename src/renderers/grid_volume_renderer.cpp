#include "renderers/grid_volume_renderer.hpp"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>

bool GridVolumeRenderer::Init() {
    if (!shader_.InitFromFiles("resources/shaders/grid_volume.vert",
                               "resources/shaders/grid_volume.frag")) {
        return false;
    }

    // Attribute-less fullscreen triangle (vertices come from gl_VertexID),
    // but core profile still requires a bound VAO to draw.
    glGenVertexArrays(1, &vao_);

    UploadTransferFunction();

    // Show the first registered volume, if any.
    if (!sources_.empty() && !SelectVolume(0)) return false;
    return true;
}

void GridVolumeRenderer::Shutdown() {
    shader_.Destroy();
    if (vao_)         { glDeleteVertexArrays(1, &vao_);     vao_ = 0; }
    if (volumeTex_)   { glDeleteTextures(1, &volumeTex_);   volumeTex_ = 0; }
    if (transferTex_) { glDeleteTextures(1, &transferTex_); transferTex_ = 0; }
}

int GridVolumeRenderer::AddVolume(const std::string& name, const std::string& rawPath) {
    sources_.push_back({name, rawPath});
    return int(sources_.size()) - 1;
}

int GridVolumeRenderer::AddVolumesFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<VolumeSource> found;
    for (const auto& entry : fs::directory_iterator(fs::current_path() / dir, ec)) {
        const fs::path& kmd = entry.path();
        if (kmd.extension() != ".kmd") continue;
        fs::path raw = kmd.stem();                 // "carp.raw"
        if (raw.extension() != ".raw") continue;
        found.push_back({raw.stem().string(), (fs::path(dir) / raw).string()});
    }
    if (ec) std::cerr << "Could not read volume directory " << dir << ": " << ec.message() << std::endl;

    std::sort(found.begin(), found.end(),
              [](const VolumeSource& a, const VolumeSource& b) { return a.name < b.name; });
    for (auto& f : found) sources_.push_back(std::move(f));
    return int(found.size());
}

bool GridVolumeRenderer::SelectVolume(int index) {
    if (index < 0 || index >= int(sources_.size())) return false;

    Volume volume;
    if (!LoadVolume(sources_[index].path, &volume)) return false;

    SetVolume(volume.voxels, volume.width, volume.height, volume.depth);
    glm::vec3 half = 0.5f * volume.Extent();
    params.boundsMin = -half;
    params.boundsMax = half;
    selectedSource_ = index;
    return true;
}

void GridVolumeRenderer::SetVolume(const std::vector<float>& voxels, int width, int height, int depth) {
    if (width <= 0 || height <= 0 || depth <= 0 ||
        voxels.size() != size_t(width) * height * depth) {
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

void GridVolumeRenderer::UploadTransferFunction() {
    UploadTable(transferFunction.Bake(256));
}

void GridVolumeRenderer::UploadTable(const std::vector<glm::vec4>& table) {
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

void GridVolumeRenderer::Render(const FrameContext& ctx) {
    if (!shader_ || !volumeTex_) return;

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

    shader_.Use();
    shader_.SetMat4("u_invViewProj", glm::inverse(ctx.camera.ViewProjection()));
    shader_.SetVec3("u_cameraPos", ctx.camera.position);
    shader_.SetVec3("u_boundsMin", params.boundsMin);
    shader_.SetVec3("u_boundsMax", params.boundsMax);
    shader_.SetInt("u_maxSteps", params.maxSteps);
    shader_.SetFloat("u_stepSize", params.stepSize);
    shader_.SetFloat("u_densityScale", params.densityScale);
    shader_.SetFloat("u_opacityCutoff", params.opacityCutoff);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, volumeTex_);
    shader_.SetInt("u_volume", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, transferTex_);
    shader_.SetInt("u_transfer", 1);

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

void GridVolumeRenderer::DrawUI() {
    if (!sources_.empty()) {
        const char* current = selectedSource_ >= 0 ? sources_[selectedSource_].name.c_str() : "(none)";
        if (ImGui::BeginCombo("Volume", current)) {
            for (int i = 0; i < int(sources_.size()); ++i) {
                if (ImGui::Selectable(sources_[i].name.c_str(), i == selectedSource_) && i != selectedSource_) {
                    SelectVolume(i);
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SliderInt("Max steps", &params.maxSteps, 16, 1024);
    ImGui::SliderFloat("Step size", &params.stepSize, 0.0005f, 0.05f, "%.4f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Density", &params.densityScale, 0.0f, 10.0f);
    ImGui::SliderFloat("Opacity cutoff", &params.opacityCutoff, 0.5f, 1.0f);

    if (ImGui::CollapsingHeader("Transfer function", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (transferFunction.DrawEditor()) UploadTransferFunction();
    }
}

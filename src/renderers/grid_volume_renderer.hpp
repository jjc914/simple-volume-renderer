#ifndef RENDERERS_GRID_VOLUME_RENDERER_HPP
#define RENDERERS_GRID_VOLUME_RENDERER_HPP

#include "opengl/shader.hpp"
#include "renderers/renderer.hpp"
#include "transfer_function.hpp"
#include "volume.hpp"

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>


// Direct volume renderer for a dense scalar grid. Ray-marches a 3D texture in
// a fragment shader (no compute needed, so it works on macOS GL 3.3/4.1).
//
//   GridVolumeRenderer vr;
//   vr.AddVolumesFromDirectory("resources/volumes");   // no GL needed yet
//   vr.Init();                                         // needs a GL context; shows volume 0
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
    TransferFunction transferFunction = TransferFunction::Default();

    GridVolumeRenderer() = default;
    ~GridVolumeRenderer() override { Shutdown(); }

    bool Init() override;
    void Shutdown() override;
    void Render(const FrameContext& ctx) override;
    void DrawUI() override;
    const char* Name() const override { return "Grid Volume"; }

    // Register a volume file (see LoadVolume) to pick from in the UI. Nothing
    // is read until it is selected (Init() selects the first one). Returns its
    // index.
    int AddVolume(const std::string& name, const std::string& rawPath);

    // Register every "<name>.raw" that has a "<name>.raw.kmd" next to it in
    // `dir` (relative to the working directory), sorted by name. Returns how
    // many were found.
    int AddVolumesFromDirectory(const std::string& dir);

    // Load the registered volume `index`, upload it and fit the bounds to its
    // proportions. Returns false (keeping the current volume) on failure.
    bool SelectVolume(int index);

    // Upload a width*height*depth scalar field, x varying fastest.
    void SetVolume(const std::vector<float>& voxels, int width, int height, int depth);

    // Bake `transferFunction` and upload it. The UI does this automatically
    // when edited; call it after changing the function from code.
    void UploadTransferFunction();

private:
    void UploadTable(const std::vector<glm::vec4>& table);

    struct VolumeSource {
        std::string name;
        std::string path;
    };
    std::vector<VolumeSource> sources_;
    int selectedSource_ = -1;

    gl::Shader shader_;
    GLuint vao_ = 0;
    GLuint volumeTex_ = 0;
    GLuint transferTex_ = 0;
    std::array<int, 3> dims_{0, 0, 0};
};

#endif

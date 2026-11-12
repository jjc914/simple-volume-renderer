#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "camera/camera.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

// Where a renderer draws.
struct RenderTarget {
    GLuint framebuffer = 0;     // 0 = default framebuffer
    GLuint colorTexture = 0;    // optional
    GLuint depthTexture = 0;    // optional
    int width = 0;
    int height = 0;
};

// Per-frame state passed to Render().
struct FrameContext {
    CameraState camera;
    RenderTarget target;
    float time = 0.0f;
    float deltaTime = 0.0f;
};

// ---------------------------------------------------------------------------
// Renderer interface
// ---------------------------------------------------------------------------
class Renderer {
public:
    virtual ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Compile shaders, allocate buffers/textures. Return false on failure.
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    // Output size changed; recreate any size-dependent resources.
    virtual void Resize(int width, int height) {}

    // Draw one frame into ctx.target. Must restore any GL state it changes
    // (blend, depth test, bound FBO/VAO/program) so renderers can be chained.
    virtual void Render(const FrameContext& ctx) = 0;

    // Hook for tweakable parameters; subclasses call ImGui here. Default: no
    // parameters.
    virtual void DrawUI() {}

    virtual const char* Name() const = 0;

    // Whether Render() writes depth into ctx.target.depthTexture.
    virtual bool WritesDepth() const { return false; }

protected:
    Renderer() = default;
};

#endif

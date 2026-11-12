#ifndef OPENGL_SHADER_HPP
#define OPENGL_SHADER_HPP

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace gl {

// RAII wrapper around a linked GLSL program (vertex + fragment).
// Move-only; the program is deleted on destruction.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Compile and link from GLSL source strings. Returns false on failure
    // (errors are printed to stderr). Any previous program is released.
    bool Init(const std::string& vertexSrc, const std::string& fragmentSrc);

    // Load GLSL from files (paths relative to the working directory, see
    // ReadFile) then compile and link. Returns false if a file can't be read
    // or is empty, or if compilation/linking fails.
    bool InitFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void Destroy();

    void Use() const;
    GLuint Id() const { return program_; }
    explicit operator bool() const { return program_ != 0; }

    // Uniform location, cached. Returns -1 if the uniform doesn't exist
    // (or was optimized out); GL silently ignores sets to -1.
    GLint Location(const std::string& name) const;

    // The Set* helpers require this program to be currently bound (call Use()).
    void SetInt(const std::string& name, int v) const;
    void SetBool(const std::string& name, bool v) const;
    void SetFloat(const std::string& name, float v) const;
    void SetVec2(const std::string& name, const glm::vec2& v) const;
    void SetVec3(const std::string& name, const glm::vec3& v) const;
    void SetVec4(const std::string& name, const glm::vec4& v) const;
    void SetMat3(const std::string& name, const glm::mat3& m) const;
    void SetMat4(const std::string& name, const glm::mat4& m) const;

private:
    GLuint program_ = 0;
    mutable std::unordered_map<std::string, GLint> uniformCache_;
};

}  // namespace gl

#endif

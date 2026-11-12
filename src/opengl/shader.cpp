#include "opengl/shader.hpp"

#include "file.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <utility>

namespace gl {

namespace {

GLuint Compile(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 0 ? len : 1, '\0');
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                  << " shader compile error:\n" << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::~Shader() { Destroy(); }

Shader::Shader(Shader&& other) noexcept
    : program_(std::exchange(other.program_, 0)),
      uniformCache_(std::move(other.uniformCache_)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        Destroy();
        program_ = std::exchange(other.program_, 0);
        uniformCache_ = std::move(other.uniformCache_);
    }
    return *this;
}

bool Shader::InitFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    Destroy();

    std::string vertexSrc = ReadFile(vertexPath);
    std::string fragmentSrc = ReadFile(fragmentPath);
    if (vertexSrc.empty() || fragmentSrc.empty()) {
        std::cerr << "Shader source empty or unreadable (vertex: " << vertexPath
                  << ", fragment: " << fragmentPath << ")" << std::endl;
        return false;
    }
    return Init(vertexSrc, fragmentSrc);
}

bool Shader::Init(const std::string& vertexSrc, const std::string& fragmentSrc) {
    Destroy();

    GLuint vs = Compile(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
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
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 0 ? len : 1, '\0');
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << "Program link error:\n" << log << std::endl;
        glDeleteProgram(program);
        return false;
    }

    program_ = program;
    return true;
}

void Shader::Destroy() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
}

void Shader::Use() const { glUseProgram(program_); }

GLint Shader::Location(const std::string& name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    GLint loc = glGetUniformLocation(program_, name.c_str());
    uniformCache_.emplace(name, loc);
    return loc;
}

void Shader::SetInt(const std::string& name, int v) const { glUniform1i(Location(name), v); }
void Shader::SetBool(const std::string& name, bool v) const { glUniform1i(Location(name), v ? 1 : 0); }
void Shader::SetFloat(const std::string& name, float v) const { glUniform1f(Location(name), v); }
void Shader::SetVec2(const std::string& name, const glm::vec2& v) const { glUniform2fv(Location(name), 1, glm::value_ptr(v)); }
void Shader::SetVec3(const std::string& name, const glm::vec3& v) const { glUniform3fv(Location(name), 1, glm::value_ptr(v)); }
void Shader::SetVec4(const std::string& name, const glm::vec4& v) const { glUniform4fv(Location(name), 1, glm::value_ptr(v)); }
void Shader::SetMat3(const std::string& name, const glm::mat3& m) const { glUniformMatrix3fv(Location(name), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::SetMat4(const std::string& name, const glm::mat4& m) const { glUniformMatrix4fv(Location(name), 1, GL_FALSE, glm::value_ptr(m)); }

}  // namespace gl

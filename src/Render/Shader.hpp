#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace KiloScope::Render {

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(Shader&& o) noexcept : prog_(o.prog_), uniformCache_(std::move(o.uniformCache_)) { o.prog_ = 0; }
    Shader& operator=(Shader&& o) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Use() const { glUseProgram(prog_); }
    GLuint Id() const { return prog_; }

    void Set(const char* n, const glm::mat4& m) const;
    void Set(const char* n, const glm::vec2& v) const;
    void Set(const char* n, const glm::vec3& v) const;
    void Set(const char* n, const glm::vec4& v) const;
    void Set(const char* n, float f) const;
    void Set(const char* n, int i) const;

private:
    GLuint prog_ = 0;
    mutable std::unordered_map<std::string, GLint> uniformCache_;

    GLint Loc(const char* name) const;
    static GLuint Compile(GLenum type, const std::string& src);
    static std::string ReadFile(const std::string& path);
};

} // namespace KiloScope::Render

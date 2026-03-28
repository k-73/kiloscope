#include "Shader.hpp"
#include "Core/Log.hpp"
#include <fstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace Kilo::Render {

// Fallback shader: renders everything as magenta to signal missing/broken shaders
static constexpr const char* kFallbackVert = R"(
#version 450
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
void main() { gl_Position = uViewProj * vec4(aPos, 1.0); }
)";

static constexpr const char* kFallbackFrag = R"(
#version 450
out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.0, 1.0, 1.0); }
)";

static GLuint CompileSource(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static GLuint LinkFallback() {
    auto vs = CompileSource(GL_VERTEX_SHADER, kFallbackVert);
    auto fs = CompileSource(GL_FRAGMENT_SHADER, kFallbackFrag);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    auto vertSrc = ReadFile(vertPath);
    auto fragSrc = ReadFile(fragPath);

    if (vertSrc.empty() || fragSrc.empty()) {
        Log::Render().error("Shader file missing: {} / {} — using fallback", vertPath, fragPath);
        prog_ = LinkFallback();
        return;
    }

    try {
        auto vs = Compile(GL_VERTEX_SHADER, vertSrc);
        auto fs = Compile(GL_FRAGMENT_SHADER, fragSrc);
        prog_ = glCreateProgram();
        glAttachShader(prog_, vs);
        glAttachShader(prog_, fs);
        glLinkProgram(prog_);

        GLint ok;
        glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[1024];
            glGetProgramInfoLog(prog_, sizeof(buf), nullptr, buf);
            glDeleteProgram(prog_); glDeleteShader(vs); glDeleteShader(fs);
            prog_ = 0;
            throw std::runtime_error(std::string("Shader link: ") + buf);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        Log::Render().info("Shader linked: {} + {}", vertPath, fragPath);
    } catch (const std::exception& e) {
        Log::Render().error("Shader failed: {} — using fallback", e.what());
        if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
        prog_ = LinkFallback();
    }
}

Shader::~Shader() { if (prog_) glDeleteProgram(prog_); }

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (prog_) glDeleteProgram(prog_);
        prog_ = std::exchange(o.prog_, 0);
        uniformCache_ = std::move(o.uniformCache_);
    }
    return *this;
}

GLint Shader::Loc(const char* name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    GLint loc = glGetUniformLocation(prog_, name);
    uniformCache_[name] = loc;
    return loc;
}

void Shader::Set(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(Loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::Set(const char* n, const glm::mat3& m) const { glUniformMatrix3fv(Loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::Set(const char* n, const glm::vec2& v) const { glUniform2fv(Loc(n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, const glm::vec3& v) const { glUniform3fv(Loc(n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, const glm::vec4& v) const { glUniform4fv(Loc(n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, float f)            const { glUniform1f(Loc(n), f); }
void Shader::Set(const char* n, int i)              const { glUniform1i(Loc(n), i); }
void Shader::Set(const char* n, unsigned int u)     const { glUniform1ui(Loc(n), u); }

GLuint Shader::Compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        glDeleteShader(s);
        Log::Render().error("Shader compile failed: {}", buf);
        throw std::runtime_error(std::string("Shader compile: ") + buf);
    }
    return s;
}

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        Log::Render().error("Cannot open shader: {}", path);
        return {};
    }
    return {std::istreambuf_iterator<char>(f), {}};
}

} // namespace Kilo::Render

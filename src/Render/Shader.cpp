#include "Shader.hpp"
#include <fstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace KiloScope::Render {

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    auto vs = Compile(GL_VERTEX_SHADER, ReadFile(vertPath));
    auto fs = Compile(GL_FRAGMENT_SHADER, ReadFile(fragPath));
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);

    GLint ok;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
        glDeleteProgram(prog_); glDeleteShader(vs); glDeleteShader(fs);
        prog_ = 0;
        throw std::runtime_error(std::string("Shader link: ") + log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() { if (prog_) glDeleteProgram(prog_); }

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) { if (prog_) glDeleteProgram(prog_); prog_ = o.prog_; o.prog_ = 0; }
    return *this;
}

void Shader::Set(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(prog_, n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::Set(const char* n, const glm::vec2& v) const { glUniform2fv(glGetUniformLocation(prog_, n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, const glm::vec3& v) const { glUniform3fv(glGetUniformLocation(prog_, n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, const glm::vec4& v) const { glUniform4fv(glGetUniformLocation(prog_, n), 1, glm::value_ptr(v)); }
void Shader::Set(const char* n, float f)            const { glUniform1f(glGetUniformLocation(prog_, n), f); }
void Shader::Set(const char* n, int i)              const { glUniform1i(glGetUniformLocation(prog_, n), i); }

GLuint Shader::Compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Shader compile: ") + log);
    }
    return s;
}

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open shader: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

} // namespace KiloScope::Render

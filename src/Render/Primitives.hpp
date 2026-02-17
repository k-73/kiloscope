#pragma once
#include "Shader.hpp"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace KiloScope::Render {

class Primitives {
public:
    ~Primitives();
    void Init(const std::string& shaderDir);

    void Begin(const glm::mat4& view, const glm::mat4& proj,
               const glm::vec3& camPos, const glm::vec3& lightDir, int vpW, int vpH);
    void FlushLines();

    void DrawLine(const glm::vec3& a, const glm::vec3& b,
                  const glm::vec4& color, float width = 2.5f);
    void DrawArrow(const glm::vec3& from, const glm::vec3& to,
                   const glm::vec4& color, float shaftR = 0.02f, float headR = 0.06f);
    void DrawSphere(const glm::vec3& center, float radius,
                    const glm::vec4& color, int seg = 32);
    void DrawCylinder(const glm::vec3& a, const glm::vec3& b,
                      float radius, const glm::vec4& color, int seg = 16);
    void DrawArc(const glm::vec3& center, const glm::vec3& axis,
                 const glm::vec3& startDir, float radius,
                 float angleDeg, const glm::vec4& color, int seg = 32);
    void DrawAxes(const glm::vec3& origin, float len = 1.f);

private:
    struct MeshVert { glm::vec3 pos, normal; };
    struct LineVert { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; };

    Shader meshShader_, lineShader_;
    GLuint meshVao_ = 0, meshVbo_ = 0, lineVao_ = 0, lineVbo_ = 0;
    glm::mat4 view_, proj_;
    glm::vec3 camPos_, lightDir_;
    int vpW_ = 1, vpH_ = 1;
    std::vector<LineVert> lineBatch_;
    float lineWidth_ = 2.5f;

    void UploadMesh(const std::vector<MeshVert>& v);
    void SetMeshUniforms(const glm::vec4& color, bool unlit = false);
    void BuildSphere(std::vector<MeshVert>& out, const glm::vec3& c, float r, int seg);
    void BuildCylinder(std::vector<MeshVert>& out, const glm::vec3& a, const glm::vec3& b, float r, int seg);
    void BuildCone(std::vector<MeshVert>& out, const glm::vec3& base, const glm::vec3& tip, float r, int seg);
};

} // namespace KiloScope::Render

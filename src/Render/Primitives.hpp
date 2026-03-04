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

    // ── transform stack ─────────────────────────────────────────────
    void PushMatrix();
    void PopMatrix();
    void ResetMatrix();
    void Translate(const glm::vec3& offset);
    void Translate(float x, float y, float z);
    void Rotate(float angleDeg, const glm::vec3& axis);
    void RotateX(float angleDeg);
    void RotateY(float angleDeg);
    void RotateZ(float angleDeg);
    void Scale(const glm::vec3& s);
    void Scale(float s);

    // ── lines ───────────────────────────────────────────────────────
    void DrawLine(const glm::vec3& a, const glm::vec3& b,
                  const glm::vec4& color, float width = 2.5f);
    void DrawArc(const glm::vec3& center, const glm::vec3& axis,
                 const glm::vec3& startDir, float radius,
                 float angleDeg, const glm::vec4& color, int seg = 32);
    void DrawCircle(const glm::vec3& center, const glm::vec3& axis,
                    float radius, const glm::vec4& color, int seg = 32);

    // ── basic geometry ──────────────────────────────────────────────
    void DrawTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                      const glm::vec4& color);
    void DrawQuad(const glm::vec3& a, const glm::vec3& b,
                  const glm::vec3& c, const glm::vec3& d, const glm::vec4& color);
    void DrawPlane(const glm::vec3& center, const glm::vec3& normal,
                   const glm::vec2& size, const glm::vec4& color);

    // ── mesh primitives ─────────────────────────────────────────────
    void DrawSphere(const glm::vec3& center, float radius,
                    const glm::vec4& color, int seg = 32);
    void DrawBox(const glm::vec3& center, const glm::vec3& size,
                 const glm::vec4& color);
    void DrawCube(const glm::vec3& center, float size,
                  const glm::vec4& color);
    void DrawCylinder(const glm::vec3& a, const glm::vec3& b,
                      float radius, const glm::vec4& color, int seg = 16);
    void DrawCone(const glm::vec3& base, const glm::vec3& tip,
                  float radius, const glm::vec4& color, int seg = 24);
    void DrawCapsule(const glm::vec3& a, const glm::vec3& b,
                     float radius, const glm::vec4& color, int seg = 16);
    void DrawTorus(const glm::vec3& center, const glm::vec3& axis,
                   float majorR, float minorR, const glm::vec4& color, int seg = 32);
    void DrawDisk(const glm::vec3& center, const glm::vec3& normal,
                  float radius, const glm::vec4& color, int seg = 32);
    void DrawRing(const glm::vec3& center, const glm::vec3& normal,
                  float innerR, float outerR, const glm::vec4& color, int seg = 32);

    // ── composite primitives ────────────────────────────────────────
    void DrawArrow(const glm::vec3& from, const glm::vec3& to,
                   const glm::vec4& color, float shaftR = 0.02f, float headR = 0.06f);
    void DrawAxes(const glm::vec3& origin, float len = 1.f);
    void DrawPoint(const glm::vec3& pos, const glm::vec4& color, float size = 0.05f);

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
    std::vector<glm::mat4> matStack_ = {glm::mat4(1.f)};

    const glm::mat4& Mat() const { return matStack_.back(); }
    glm::vec3 XformPoint(const glm::vec3& p) const;
    glm::vec3 XformDir(const glm::vec3& d) const;

    void UploadMesh(const std::vector<MeshVert>& v);
    void SetMeshUniforms(const glm::vec4& color, bool unlit = false);

    template <typename MeshT>
    void AppendMesh(std::vector<MeshVert>& out, const MeshT& mesh, const glm::mat4& xform);
};

} // namespace KiloScope::Render

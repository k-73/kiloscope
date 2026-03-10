#include "Primitives.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <generator/SphereMesh.hpp>
#include <generator/BoxMesh.hpp>
#include <generator/CappedCylinderMesh.hpp>
#include <generator/CappedConeMesh.hpp>
#include <generator/CapsuleMesh.hpp>
#include <generator/TorusMesh.hpp>
#include <generator/DiskMesh.hpp>
#include <cmath>

namespace Kilo::Render {

// ── lifecycle ────────────────────────────────────────────────────────

Primitives::~Primitives() {
    if (meshVao_) glDeleteVertexArrays(1, &meshVao_);
    if (meshVbo_) glDeleteBuffers(1, &meshVbo_);
    if (lineVao_) glDeleteVertexArrays(1, &lineVao_);
    if (lineVbo_) glDeleteBuffers(1, &lineVbo_);
}

static void SetupVao(GLuint vao, GLuint vbo, GLsizei stride,
                     std::initializer_list<std::pair<GLuint, std::pair<GLint, GLuint>>> attrs) {
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
    for (auto& [idx, spec] : attrs) {
        glEnableVertexArrayAttrib(vao, idx);
        glVertexArrayAttribFormat(vao, idx, spec.first, GL_FLOAT, GL_FALSE, spec.second);
        glVertexArrayAttribBinding(vao, idx, 0);
    }
}

void Primitives::Init(const std::string& dir) {
    meshShader_ = Shader(dir + "/Basic.vert", dir + "/Basic.frag");
    lineShader_ = Shader(dir + "/Line.vert",  dir + "/Line.frag");

    glCreateVertexArrays(1, &meshVao_); glCreateBuffers(1, &meshVbo_);
    SetupVao(meshVao_, meshVbo_, sizeof(MeshVert), {
        {0, {3, offsetof(MeshVert, pos)}}, {1, {3, offsetof(MeshVert, normal)}}});

    glCreateVertexArrays(1, &lineVao_); glCreateBuffers(1, &lineVbo_);
    SetupVao(lineVao_, lineVbo_, sizeof(LineVert), {
        {0, {3, offsetof(LineVert, pos)}}, {1, {3, offsetof(LineVert, otherEnd)}},
        {2, {2, offsetof(LineVert, expand)}}, {3, {4, offsetof(LineVert, color)}}});
}

void Primitives::Begin(const glm::mat4& view, const glm::mat4& proj,
                       const glm::vec3& camPos, const glm::vec3& lightDir, int vpW, int vpH) {
    view_ = view; proj_ = proj; camPos_ = camPos;
    lightDir_ = glm::normalize(lightDir);
    vpW_ = vpW; vpH_ = vpH;
    lineBatch_.clear();
    matStack_ = {glm::mat4(1.f)};
}

void Primitives::SetMeshUniforms(const glm::vec4& color, bool unlit) {
    meshShader_.Use();
    meshShader_.Set("uModel", glm::mat4(1.f));
    meshShader_.Set("uView", view_); meshShader_.Set("uProj", proj_);
    meshShader_.Set("uColor", color);
    meshShader_.Set("uLightDir", lightDir_); meshShader_.Set("uCamPos", camPos_);
    meshShader_.Set("uUnlit", unlit ? 1 : 0);
}

void Primitives::UploadMesh(const std::vector<MeshVert>& v) {
    glNamedBufferData(meshVbo_, v.size() * sizeof(MeshVert), v.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(meshVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size()));
    glBindVertexArray(0);
}

// ── helpers ──────────────────────────────────────────────────────────

template <typename MeshT>
void Primitives::AppendMesh(std::vector<MeshVert>& out, const MeshT& mesh,
                            const glm::mat4& xform) {
    auto nmat = glm::transpose(glm::inverse(glm::mat3(xform)));
    std::vector<MeshVert> indexed;
    for (auto it = mesh.vertices(); !it.done(); it.next()) {
        auto v = it.generate();
        indexed.push_back({
            glm::vec3(xform * glm::vec4(glm::vec3(v.position), 1.f)),
            glm::normalize(nmat * glm::vec3(v.normal))
        });
    }
    for (auto it = mesh.triangles(); !it.done(); it.next()) {
        auto t = it.generate();
        out.push_back(indexed[t.vertices[0]]);
        out.push_back(indexed[t.vertices[1]]);
        out.push_back(indexed[t.vertices[2]]);
    }
}

static glm::mat4 AxisRotation(const glm::vec3& dir) {
    auto n = glm::normalize(dir);
    float dot = glm::clamp(glm::dot(glm::vec3(0, 0, 1), n), -1.f, 1.f);
    if (dot < -0.999f)
        return glm::rotate(glm::mat4(1.f), glm::pi<float>(), glm::vec3(1, 0, 0));
    if (dot > 0.999f)
        return glm::mat4(1.f);
    return glm::rotate(glm::mat4(1.f), std::acos(dot),
                       glm::normalize(glm::cross(glm::vec3(0, 0, 1), n)));
}

static glm::mat4 ZAlign(const glm::vec3& a, const glm::vec3& b) {
    return glm::translate(glm::mat4(1.f), (a + b) * 0.5f) * AxisRotation(b - a);
}

static glm::mat4 AxisTransform(const glm::vec3& center, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.f), center) * AxisRotation(axis);
}

static glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    return glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f
                                        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
}

glm::vec3 Primitives::XformPoint(const glm::vec3& p) const {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

glm::vec3 Primitives::XformDir(const glm::vec3& d) const {
    return glm::mat3(Mat()) * d;
}

// ── transform stack ──────────────────────────────────────────────────

void Primitives::PushMatrix()  { matStack_.push_back(Mat()); }
void Primitives::PopMatrix()   { if (matStack_.size() > 1) matStack_.pop_back(); }
void Primitives::ResetMatrix() { matStack_.back() = glm::mat4(1.f); }

void Primitives::Translate(const glm::vec3& offset) {
    matStack_.back() = matStack_.back() * glm::translate(glm::mat4(1.f), offset);
}
void Primitives::Translate(float x, float y, float z) { Translate({x, y, z}); }

void Primitives::Rotate(float angleDeg, const glm::vec3& axis) {
    matStack_.back() = matStack_.back() * glm::rotate(glm::mat4(1.f), glm::radians(angleDeg), axis);
}
void Primitives::RotateX(float deg) { Rotate(deg, {1, 0, 0}); }
void Primitives::RotateY(float deg) { Rotate(deg, {0, 1, 0}); }
void Primitives::RotateZ(float deg) { Rotate(deg, {0, 0, 1}); }

void Primitives::Scale(const glm::vec3& s) {
    matStack_.back() = matStack_.back() * glm::scale(glm::mat4(1.f), s);
}
void Primitives::Scale(float s) { Scale({s, s, s}); }

// ── lines ────────────────────────────────────────────────────────────

void Primitives::BatchLine(const glm::vec3& a, const glm::vec3& b,
                            const glm::vec4& color, float width) {
    if (!lineBatch_.empty() && width != lineWidth_)
        FlushLines();
    lineWidth_ = width;
    lineBatch_.push_back({a, b, {-1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 1}, color});
    lineBatch_.push_back({a, b, {-1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 1}, color});
    lineBatch_.push_back({a, b, {-1, 1}, color});
}

void Primitives::DrawLine(const glm::vec3& a, const glm::vec3& b,
                           const glm::vec4& color, float width) {
    BatchLine(XformPoint(a), XformPoint(b), color, width);
}

void Primitives::FlushLines() {
    if (lineBatch_.empty()) return;
    lineShader_.Use();
    lineShader_.Set("uView", view_); lineShader_.Set("uProj", proj_);
    lineShader_.Set("uViewportSize", glm::vec2(vpW_, vpH_));
    lineShader_.Set("uLineWidth", lineWidth_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glNamedBufferData(lineVbo_, lineBatch_.size() * sizeof(LineVert), lineBatch_.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(lineVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(lineBatch_.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    lineBatch_.clear();
}

void Primitives::DrawArc(const glm::vec3& center, const glm::vec3& axis,
                          const glm::vec3& startDir, float radius,
                          float angleDeg, const glm::vec4& color, int seg) {
    auto tc = XformPoint(center);
    auto ta = glm::normalize(XformDir(axis));
    auto ts = glm::normalize(XformDir(startDir));
    float step = glm::radians(angleDeg) / static_cast<float>(seg);
    for (int i = 0; i < seg; ++i) {
        auto r0 = glm::rotate(glm::mat4(1), step * i,       ta);
        auto r1 = glm::rotate(glm::mat4(1), step * (i + 1), ta);
        BatchLine(tc + glm::vec3(r0 * glm::vec4(ts * radius, 0)),
                  tc + glm::vec3(r1 * glm::vec4(ts * radius, 0)), color, 2.f);
    }
}

void Primitives::DrawCircle(const glm::vec3& center, const glm::vec3& axis,
                              float radius, const glm::vec4& color, int seg) {
    DrawArc(center, axis, Perpendicular(axis), radius, 360.f, color, seg);
}

// ── basic geometry ───────────────────────────────────────────────────

void Primitives::DrawTriangle(const glm::vec3& a, const glm::vec3& b,
                                const glm::vec3& c, const glm::vec4& color) {
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto normal = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal}});
}

void Primitives::DrawQuad(const glm::vec3& a, const glm::vec3& b,
                            const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c), td = XformPoint(d);
    auto normal = glm::normalize(glm::cross(tb - ta, td - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal},
                {ta, normal}, {tc, normal}, {td, normal}});
}

void Primitives::DrawPlane(const glm::vec3& center, const glm::vec3& normal,
                             const glm::vec2& size, const glm::vec4& color) {
    auto n = glm::normalize(normal);
    auto u = Perpendicular(n);
    auto v = glm::cross(n, u);
    auto a = center + (-u * size.x - v * size.y);
    auto b = center + ( u * size.x - v * size.y);
    auto c = center + ( u * size.x + v * size.y);
    auto d = center + (-u * size.x + v * size.y);
    DrawQuad(a, b, c, d, color);
}

// ── mesh primitives ──────────────────────────────────────────────────

void Primitives::DrawSphere(const glm::vec3& center, float radius,
                              const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::SphereMesh(radius, seg, seg / 2),
               Mat() * glm::translate(glm::mat4(1.f), center));
    UploadMesh(v);
}

void Primitives::DrawBox(const glm::vec3& center, const glm::vec3& size,
                           const glm::vec4& color) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::BoxMesh({size.x, size.y, size.z}, {1, 1, 1}),
               Mat() * glm::translate(glm::mat4(1.f), center));
    UploadMesh(v);
}

void Primitives::DrawCube(const glm::vec3& center, float size,
                            const glm::vec4& color) {
    DrawBox(center, glm::vec3(size), color);
}

void Primitives::DrawCylinder(const glm::vec3& a, const glm::vec3& b,
                                float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) return;
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CappedCylinderMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(a, b));
    UploadMesh(v);
}

void Primitives::DrawCone(const glm::vec3& base, const glm::vec3& tip,
                            float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(tip - base) * 0.5f;
    if (halfLen < 1e-6f) return;
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CappedConeMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(base, tip));
    UploadMesh(v);
}

void Primitives::DrawCapsule(const glm::vec3& a, const glm::vec3& b,
                               float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { DrawSphere((a + b) * 0.5f, radius, color, seg); return; }
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CapsuleMesh(radius, halfLen, seg, 1, seg / 2),
               Mat() * ZAlign(a, b));
    UploadMesh(v);
}

void Primitives::DrawTorus(const glm::vec3& center, const glm::vec3& axis,
                             float majorR, float minorR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(v);
}

void Primitives::DrawDisk(const glm::vec3& center, const glm::vec3& normal,
                            float radius, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(radius, 0.0, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(v);
}

void Primitives::DrawRing(const glm::vec3& center, const glm::vec3& normal,
                            float innerR, float outerR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(v);
}

// ── composite primitives ─────────────────────────────────────────────

void Primitives::DrawArrow(const glm::vec3& from, const glm::vec3& to,
                             const glm::vec4& color, float shaftR, float headR) {
    auto dir = to - from;
    float len = glm::length(dir);
    if (len < 1e-6f) return;

    float headLen = std::min(len * 0.25f, headR * 2.5f);
    auto shaftEnd = from + dir * ((len - headLen) / len);

    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    float halfShaft = glm::length(shaftEnd - from) * 0.5f;
    float halfHead  = headLen * 0.5f;
    AppendMesh(v, generator::CappedCylinderMesh(shaftR, halfShaft, 24, 1, 1),
               Mat() * ZAlign(from, shaftEnd));
    AppendMesh(v, generator::CappedConeMesh(headR, halfHead, 24, 1, 1),
               Mat() * ZAlign(shaftEnd, to));
    UploadMesh(v);
}

void Primitives::DrawAxes(const glm::vec3& o, float len) {
    float s = len * 0.025f;
    float h = len * 0.07f;
    DrawArrow(o, o + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    DrawArrow(o, o + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    DrawArrow(o, o + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
}

void Primitives::DrawPoint(const glm::vec3& pos, const glm::vec4& color, float size) {
    DrawSphere(pos, size, color, 8);
}

} // namespace Kilo::Render

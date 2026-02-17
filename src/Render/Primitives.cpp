#include "Primitives.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace ks::render {

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
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
    glBindVertexArray(0);
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
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)lineBatch_.size());
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    lineBatch_.clear();
}

void Primitives::DrawLine(const glm::vec3& a, const glm::vec3& b,
                           const glm::vec4& color, float width) {
    lineWidth_ = width;
    // 6 verts = 2 tris forming a screen-space quad; all verts share same (a,b) order
    lineBatch_.push_back({a, b, {-1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 1}, color});
    lineBatch_.push_back({a, b, {-1, 0}, color});
    lineBatch_.push_back({a, b, { 1, 1}, color});
    lineBatch_.push_back({a, b, {-1, 1}, color});
}

void Primitives::DrawArrow(const glm::vec3& from, const glm::vec3& to,
                             const glm::vec4& color, float shaftR, float headR) {
    auto dir = to - from;
    float len = glm::length(dir);
    if (len < 1e-6f) return;
    float headLen = std::min(len * 0.3f, headR * 3.f);
    auto shaftEnd = from + dir * ((len - headLen) / len);

    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    BuildCylinder(v, from, shaftEnd, shaftR, 12);
    BuildCone(v, shaftEnd, to, headR, 12);
    UploadMesh(v);
}

void Primitives::DrawSphere(const glm::vec3& center, float radius,
                              const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    BuildSphere(v, center, radius, seg);
    UploadMesh(v);
}

void Primitives::DrawCylinder(const glm::vec3& a, const glm::vec3& b,
                                float radius, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    BuildCylinder(v, a, b, radius, seg);
    UploadMesh(v);
}

void Primitives::DrawArc(const glm::vec3& center, const glm::vec3& axis,
                           const glm::vec3& startDir, float radius,
                           float angleDeg, const glm::vec4& color, int seg) {
    auto nAxis = glm::normalize(axis);
    auto nStart = glm::normalize(startDir);
    float step = glm::radians(angleDeg) / (float)seg;
    for (int i = 0; i < seg; ++i) {
        auto r0 = glm::rotate(glm::mat4(1), step * i,       nAxis);
        auto r1 = glm::rotate(glm::mat4(1), step * (i + 1), nAxis);
        auto p0 = center + glm::vec3(r0 * glm::vec4(nStart * radius, 0));
        auto p1 = center + glm::vec3(r1 * glm::vec4(nStart * radius, 0));
        DrawLine(p0, p1, color, 2.f);
    }
}

void Primitives::DrawAxes(const glm::vec3& o, float len) {
    DrawArrow(o, o + glm::vec3(len, 0, 0), {1, .15f, .15f, 1});
    DrawArrow(o, o + glm::vec3(0, len, 0), {.15f, 1, .15f, 1});
    DrawArrow(o, o + glm::vec3(0, 0, len), {.3f, .3f, 1, 1});
}

// ── mesh builders ───────────────────────────────────────────────────

static glm::vec3 Perp(const glm::vec3& ax) {
    return glm::normalize(glm::cross(ax, std::abs(ax.y) < .99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0)));
}

void Primitives::BuildSphere(std::vector<MeshVert>& out, const glm::vec3& c, float r, int seg) {
    for (int i = 0; i < seg; ++i) {
        float lat0 = glm::pi<float>() * (-.5f + (float)i / seg);
        float lat1 = glm::pi<float>() * (-.5f + (float)(i+1) / seg);
        for (int j = 0; j < seg * 2; ++j) {
            float lon0 = 2.f * glm::pi<float>() * (float)j / (seg*2);
            float lon1 = 2.f * glm::pi<float>() * (float)(j+1) / (seg*2);
            auto pt = [&](float la, float lo) -> MeshVert {
                glm::vec3 n(std::cos(la)*std::cos(lo), std::sin(la), std::cos(la)*std::sin(lo));
                return {c + n*r, n};
            };
            auto a = pt(lat0,lon0), b = pt(lat1,lon0), d = pt(lat1,lon1), e = pt(lat0,lon1);
            out.insert(out.end(), {a, b, d, a, d, e});
        }
    }
}

void Primitives::BuildCylinder(std::vector<MeshVert>& out, const glm::vec3& a,
                                const glm::vec3& b, float r, int seg) {
    auto dir = b - a; float len = glm::length(dir);
    if (len < 1e-6f) return;
    auto ax = dir / len;
    auto p1 = Perp(ax), p2 = glm::cross(ax, p1);

    for (int i = 0; i < seg; ++i) {
        float a0 = 2.f * glm::pi<float>() * i / seg;
        float a1 = 2.f * glm::pi<float>() * (i+1) / seg;
        auto n0 = p1*std::cos(a0) + p2*std::sin(a0);
        auto n1 = p1*std::cos(a1) + p2*std::sin(a1);
        auto pa0 = a+n0*r, pa1 = a+n1*r, pb0 = b+n0*r, pb1 = b+n1*r;
        out.insert(out.end(), {{pa0,n0},{pb0,n0},{pb1,n1}, {pa0,n0},{pb1,n1},{pa1,n1}});
        out.insert(out.end(), {{a,-ax},{pa1,-ax},{pa0,-ax}, {b,ax},{pb0,ax},{pb1,ax}});
    }
}

void Primitives::BuildCone(std::vector<MeshVert>& out, const glm::vec3& base,
                             const glm::vec3& tip, float r, int seg) {
    auto dir = tip - base; float len = glm::length(dir);
    if (len < 1e-6f) return;
    auto ax = dir / len; float slope = r / len;
    auto p1 = Perp(ax), p2 = glm::cross(ax, p1);

    for (int i = 0; i < seg; ++i) {
        float a0 = 2.f * glm::pi<float>() * i / seg;
        float a1 = 2.f * glm::pi<float>() * (i+1) / seg;
        auto n0 = p1*std::cos(a0) + p2*std::sin(a0);
        auto n1 = p1*std::cos(a1) + p2*std::sin(a1);
        auto cn0 = glm::normalize(n0 + ax*slope);
        auto cn1 = glm::normalize(n1 + ax*slope);
        auto cnt = glm::normalize(cn0 + cn1);
        auto q0 = base+n0*r, q1 = base+n1*r;
        out.insert(out.end(), {{q0,cn0},{q1,cn1},{tip,cnt}});
        out.insert(out.end(), {{base,-ax},{q1,-ax},{q0,-ax}});
    }
}

} // namespace ks::render

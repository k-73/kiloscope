#include "Primitives.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace KiloScope::Render {

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

    float headLen = std::min(len * 0.25f, headR * 2.5f);
    auto shaftEnd = from + dir * ((len - headLen) / len);

    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    v.reserve(24 * 6 * 2);
    BuildCylinder(v, from, shaftEnd, shaftR, 24);
    BuildCone(v, shaftEnd, to, headR, 24);
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
        DrawLine(center + glm::vec3(r0 * glm::vec4(nStart * radius, 0)),
                 center + glm::vec3(r1 * glm::vec4(nStart * radius, 0)), color, 2.f);
    }
}

void Primitives::DrawAxes(const glm::vec3& o, float len) {
    float s = len * 0.025f;   // shaft radius proportional to length
    float h = len * 0.07f;    // head radius
    DrawArrow(o, o + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    DrawArrow(o, o + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    DrawArrow(o, o + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
}

// ── mesh builders ───────────────────────────────────────────────────

static void Basis(const glm::vec3& ax, glm::vec3& u, glm::vec3& v) {
    u = glm::normalize(glm::cross(ax, std::abs(ax.y) < .99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0)));
    v = glm::cross(ax, u);
}

static glm::vec3 Circle(const glm::vec3& u, const glm::vec3& v, int i, int seg) {
    float a = 2.f * glm::pi<float>() * (float)i / (float)seg;
    return u * std::cos(a) + v * std::sin(a);
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
            // CCW winding when viewed from outside
            out.insert(out.end(), {a, b, d, a, d, e});
        }
    }
}

void Primitives::BuildCylinder(std::vector<MeshVert>& out, const glm::vec3& a,
                                const glm::vec3& b, float r, int seg) {
    auto dir = b - a; float len = glm::length(dir);
    if (len < 1e-6f) return;
    auto ax = dir / len;
    glm::vec3 u, v; Basis(ax, u, v);

    for (int i = 0; i < seg; ++i) {
        auto n0 = Circle(u, v, i,   seg);
        auto n1 = Circle(u, v, i+1, seg);
        auto pa0 = a+n0*r, pa1 = a+n1*r, pb0 = b+n0*r, pb1 = b+n1*r;
        // Side quads — CCW from outside (normal points outward)
        out.insert(out.end(), {{pa0,n0},{pa1,n1},{pb1,n1}, {pa0,n0},{pb1,n1},{pb0,n0}});
        // End caps — CCW from cap face direction
        out.insert(out.end(), {{a,-ax},{pa1,-ax},{pa0,-ax}});  // bottom cap
        out.insert(out.end(), {{b, ax},{pb0, ax},{pb1, ax}});  // top cap
    }
}

void Primitives::BuildCone(std::vector<MeshVert>& out, const glm::vec3& base,
                             const glm::vec3& tip, float r, int seg) {
    auto dir = tip - base; float len = glm::length(dir);
    if (len < 1e-6f) return;
    auto ax = dir / len;
    glm::vec3 u, v; Basis(ax, u, v);

    // Correct cone normal: for cone with radius r and height len,
    // the surface normal has radial component `len` and axial component `r`
    float nRatio = r / std::sqrt(r*r + len*len);
    float aRatio = len / std::sqrt(r*r + len*len);

    for (int i = 0; i < seg; ++i) {
        auto n0 = Circle(u, v, i,   seg);
        auto n1 = Circle(u, v, i+1, seg);
        // Cone surface normals — perpendicular to the slant surface
        auto cn0 = glm::normalize(n0 * aRatio + ax * nRatio);
        auto cn1 = glm::normalize(n1 * aRatio + ax * nRatio);
        auto cnt = glm::normalize(cn0 + cn1);
        auto q0 = base+n0*r, q1 = base+n1*r;
        // Side — CCW from outside
        out.insert(out.end(), {{q0,cn0},{q1,cn1},{tip,cnt}});
        // Base cap — CCW from -ax
        out.insert(out.end(), {{base,-ax},{q1,-ax},{q0,-ax}});
    }
}

} // namespace KiloScope::Render

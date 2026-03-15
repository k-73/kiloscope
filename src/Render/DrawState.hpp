#pragma once
// Internal header — shared state between Draw.cpp and DrawPrimitives.cpp
// NOT part of public API. Do not include from outside src/Render/.

#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include "Render/Fbo.hpp"
#include "Render/Shader.hpp"
#include <GL/glew.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Kilo::Render {

// ── types ────────────────────────────────────────────────────────────

struct MeshVert  { glm::vec3 pos, normal; glm::vec2 uv{0.f, 0.f}; };
struct LineVert  { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; uint32_t pickId; };
struct PointVert { glm::vec3 pos; glm::vec4 color; uint32_t pickId; };
struct TextEntry { glm::vec3 worldPos; glm::vec4 color; std::string text; };

// Cached indexed mesh (generated once, transformed per-use)
struct IndexedMesh {
    std::vector<glm::vec3> pos, nrm;
    std::vector<glm::vec2> uv;
    std::vector<std::array<int, 3>> tri;
};

const IndexedMesh& GetUnitSphere(int seg);
const IndexedMesh& GetUnitBox();

inline void AppendFromCache(std::vector<MeshVert>& out,
                            const IndexedMesh& mesh, const glm::mat4& xform) {
    auto nmat = glm::transpose(glm::inverse(glm::mat3(xform)));
    for (auto& tri : mesh.tri) {
        for (size_t j = 0; j < 3; ++j) {
            auto idx = tri[j];
            MeshVert v;
            v.pos    = glm::vec3(xform * glm::vec4(mesh.pos[idx], 1.f));
            v.normal = glm::normalize(nmat * mesh.nrm[idx]);
            if (!mesh.uv.empty()) v.uv = mesh.uv[idx];
            out.push_back(v);
        }
    }
}

struct MeshDraw {
    GLsizei offset, count;
    glm::vec4 color;
    int unlitMode;          // 0=lit, 1=unlit, 2=emissive, 3=glow
    uint32_t pickId;
};

// ── FBO types ────────────────────────────────────────────────────────

struct PickFbo {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;
    void Resize(int nw, int nh);
    void Bind();
    void Clear();
    uint32_t ReadPixel(int screenX, int screenY) const;
    void Destroy();
    ~PickFbo() { Destroy(); }
    PickFbo() = default;
    PickFbo(const PickFbo&) = delete;
    PickFbo& operator=(const PickFbo&) = delete;
};

// ── per-scene state ──────────────────────────────────────────────────

inline constexpr int   kMaxPointLights   = 32;    // must match MAX_POINT_LIGHTS in Basic.frag
inline constexpr float kGlowRadiusScale = 2.f;    // auto glow sphere = bounding radius * this
inline constexpr float kGlowRadiusMin   = 0.05f;  // minimum glow sphere radius
inline constexpr float kGlowAlpha       = 0.35f;  // glow sphere opacity

struct SceneData {
    // GPU resources
    Fbo fbo; PickFbo pickFbo;

    // Configuration
    Camera cam; Environment env; GridConfig gridCfg;

    // Mesh batching
    std::vector<MeshDraw> drawList;
    std::vector<MeshVert> vboAccum;

    // Line / point / text batching
    std::vector<LineVert>  lineBatch;
    std::vector<PointVert> pointBatch;
    std::vector<TextEntry> textBatch;
    float lineWidth  = 2.5f;
    float pointSize  = 4.f;

    // Transform stack
    std::vector<glm::mat4> matStack = {glm::mat4(1.f)};

    // Lights
    int numPointLights = 0;
    PointLightInfo pointLights[kMaxPointLights]{};

    // Pick state
    uint32_t hoveredPickId  = 0;
    uint32_t nextPickId     = 0;
    uint32_t lastPickId     = 0;
    uint32_t pickIdOverride = 0;
    bool     pickEnabled    = true;

    // Emissive one-shot flags
    bool  emissive       = false;
    bool  glow           = false;
    float emissiveGlowRadius = 0.f;

    // Per-draw transient
    glm::vec4 currentColor{};
    int       currentUnlitMode = 0;

    // Frame stats & guard
    Stats stats{};
    bool  meshFrameReady = false;
};

struct FrameState {
    SceneData* scene{};
    float cx{}, cy{};       // viewport origin (screen coords)
    float w{}, h{};          // viewport size
    bool hovered{}, fly{};
};

// ── shared state (GPU resources + per-frame derived) ─────────────────

inline Shader sMeshShader, sLineShader, sGridShader, sPointShader;
inline Shader sPickMeshShader, sPickLineShader, sPickPointShader;
inline GLuint sMeshVao = 0, sMeshVbo = 0;
inline GLuint sLineVao = 0, sLineVbo = 0;
inline GLuint sGridVao = 0;
inline GLuint sPointVao = 0, sPointVbo = 0;

inline glm::mat4 sView, sProj, sViewProj;
inline glm::vec3 sCamPos, sLightDir;
inline int sVpW = 1, sVpH = 1;

inline std::vector<MeshVert> sMeshScratch;
inline std::vector<MeshVert> sIndexedScratch;

inline std::unordered_map<uint32_t, std::unique_ptr<SceneData>> sScenes;
inline FrameState sFrame;

// ── scene accessor ───────────────────────────────────────────────────

inline SceneData& ctx() { return *sFrame.scene; }

// ── inline helpers ───────────────────────────────────────────────────

inline uint32_t HashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

inline uint32_t AllocPickId() {
    return ctx().pickIdOverride ? ctx().pickIdOverride : ++ctx().nextPickId;
}

inline const glm::mat4& Mat() { return ctx().matStack.back(); }

inline glm::vec3 XformPoint(const glm::vec3& p) {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

inline glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
}

inline glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    // Use X-axis fallback when v is nearly aligned with Y to avoid degenerate cross product
    auto up = (std::abs(n.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    return glm::normalize(glm::cross(n, up));
}

inline glm::mat4 AxisRotation(const glm::vec3& dir) {
    auto n = glm::normalize(dir);
    float dot = glm::clamp(glm::dot(glm::vec3(0, 0, 1), n), -1.f, 1.f);
    if (dot < -0.999f)
        return glm::rotate(glm::mat4(1.f), glm::pi<float>(), glm::vec3(1, 0, 0));
    if (dot > 0.999f)
        return glm::mat4(1.f);
    return glm::rotate(glm::mat4(1.f), std::acos(dot),
                       glm::normalize(glm::cross(glm::vec3(0, 0, 1), n)));
}

inline glm::mat4 ZAlign(const glm::vec3& a, const glm::vec3& b) {
    return glm::translate(glm::mat4(1.f), (a + b) * 0.5f) * AxisRotation(b - a);
}

inline glm::mat4 AxisTransform(const glm::vec3& center, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.f), center) * AxisRotation(axis);
}

// ── RAII pick group ──────────────────────────────────────────────────

struct PickGroup {
    bool owned;
    PickGroup() : owned(ctx().pickIdOverride == 0) {
        if (owned) { ctx().pickIdOverride = ++ctx().nextPickId; ctx().lastPickId = ctx().pickIdOverride; }
    }
    ~PickGroup() { if (owned) ctx().pickIdOverride = 0; }
};

// ── function declarations (defined in Draw.cpp, called by DrawPrimitives.cpp)
void SetMeshUniforms(const glm::vec4& color, bool unlit = false);
void UploadMesh(const std::vector<MeshVert>& v);
void BatchLine(const glm::vec3& a, const glm::vec3& b,
               const glm::vec4& color, float width);
void BatchLineGradient(const glm::vec3& a, const glm::vec3& b,
                        const glm::vec4& ca, const glm::vec4& cb, float width);
void FlushLines();
void FlushPoints();
void SetMeshFrameUniforms();

// ── AppendMesh template (must be in header) ──────────────────────────

template <typename MeshT>
void AppendMesh(std::vector<MeshVert>& out, const MeshT& mesh,
                const glm::mat4& xform) {
    auto nmat = glm::transpose(glm::inverse(glm::mat3(xform)));
    sIndexedScratch.clear();
    for (auto it = mesh.vertices(); !it.done(); it.next()) {
        auto v = it.generate();
        sIndexedScratch.push_back({
            glm::vec3(xform * glm::vec4(glm::vec3(v.position), 1.f)),
            glm::normalize(nmat * glm::vec3(v.normal)),
            glm::vec2(v.texCoord)
        });
    }
    for (auto it = mesh.triangles(); !it.done(); it.next()) {
        auto t = it.generate();
        out.push_back(sIndexedScratch[t.vertices[0]]);
        out.push_back(sIndexedScratch[t.vertices[1]]);
        out.push_back(sIndexedScratch[t.vertices[2]]);
    }
}

// ── Jacobi eigensolver ───────────────────────────────────────────────

// Jacobi eigensolver for 3x3 symmetric matrices (max 50 iterations, 1e-8 tolerance)
inline void Eigen3(const glm::mat3& A, glm::vec3& eigenvalues, glm::mat3& eigenvectors) {
    glm::mat3 D = A;
    eigenvectors = glm::mat3(1.f);
    for (int iter = 0; iter < 50; ++iter) {
        int p = 0, q = 1;
        float mx = std::abs(D[0][1]);
        if (std::abs(D[0][2]) > mx) { p = 0; q = 2; mx = std::abs(D[0][2]); }
        if (std::abs(D[1][2]) > mx) { p = 1; q = 2; mx = std::abs(D[1][2]); }
        if (mx < 1e-8f) break;
        float theta = 0.5f * std::atan2(2.f * D[p][q], D[q][q] - D[p][p]);
        float c = std::cos(theta), s = std::sin(theta);
        glm::mat3 J(1.f);
        J[p][p] = c;  J[q][q] = c;
        J[p][q] = s;  J[q][p] = -s;
        D = glm::transpose(J) * D * J;
        eigenvectors = eigenvectors * J;
    }
    eigenvalues = {D[0][0], D[1][1], D[2][2]};
}

} // namespace Kilo::Render

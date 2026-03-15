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
#include <cmath>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Kilo::Render {

// ── types ────────────────────────────────────────────────────────────

struct MeshVert  { glm::vec3 pos, normal; };
struct LineVert  { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; uint32_t pickId; };
struct PointVert { glm::vec3 pos; glm::vec4 color; uint32_t pickId; };
struct TextEntry { glm::vec3 worldPos; glm::vec4 color; std::string text; };

struct MeshDraw {
    GLsizei offset, count;
    glm::vec4 color;
    int unlitMode;      // 0=lit, 1=unlit, 2=emissive, 3=glow
    uint32_t pickId;
};

// ── FBO types ────────────────────────────────────────────────────────

struct PickFbo {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;
    void Resize(int nw, int nh);
    void Bind();
    void Clear();
    uint32_t ReadPixel(int x, int y) const;
    void Destroy();
    ~PickFbo() { Destroy(); }
    PickFbo() = default;
    PickFbo(const PickFbo&) = delete;
    PickFbo& operator=(const PickFbo&) = delete;
};

// ── per-scene state ──────────────────────────────────────────────────

struct SceneData {
    Fbo fbo; PickFbo pickFbo;
    Camera cam; Environment env; GridConfig gridCfg;
    uint32_t hoveredPickId = 0;
};

struct FrameState {
    SceneData* scene{};
    float cx{}, cy{}, w{}, h{};
    bool hovered{}, fly{};
};

// ── shared state (inline — single instance across TUs) ──────────────

inline Shader sMeshShader, sLineShader, sGridShader, sPointShader;
inline Shader sPickMeshShader, sPickLineShader, sPickPointShader;
inline GLuint sMeshVao = 0, sMeshVbo = 0;
inline GLuint sLineVao = 0, sLineVbo = 0;
inline GLuint sGridVao = 0;
inline GLuint sPointVao = 0, sPointVbo = 0;

inline glm::mat4 sView, sProj, sViewProj;
inline glm::vec3 sCamPos, sLightDir;
inline int sVpW = 1, sVpH = 1;
inline std::vector<LineVert>  sLineBatch;
inline std::vector<PointVert> sPointBatch;
inline std::vector<TextEntry> sTextBatch;
inline float sLineWidth  = 2.5f;
inline float sPointSize  = 4.f;
inline std::vector<glm::mat4> sMatStack = {glm::mat4(1.f)};
inline std::vector<MeshVert>  sMeshScratch;
inline std::vector<MeshVert>  sIndexedScratch;

inline constexpr int kMaxPointLights = 8;
inline int sNumPointLights = 0;
inline PointLightInfo sPointLights[kMaxPointLights];

inline std::vector<MeshDraw> sDrawList;
inline std::vector<MeshVert> sVboAccum;

inline std::unordered_map<uint32_t, std::unique_ptr<SceneData>> sScenes;
inline FrameState sFrame;
inline Environment* sEnv = nullptr;

inline uint32_t sNextPickId     = 0;
inline uint32_t sLastPickId     = 0;
inline uint32_t sPickIdOverride = 0;
inline bool     sPickEnabled    = true;
inline bool     sEmissive       = false;
inline bool     sGlow           = false;
inline Stats    sStats;

inline bool sMeshFrameReady = false;
inline glm::vec4 sCurrentColor;
inline int sCurrentUnlitMode = 0;

// ── inline helpers ───────────────────────────────────────────────────

inline uint32_t HashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

inline uint32_t AllocPickId() {
    return sPickIdOverride ? sPickIdOverride : ++sNextPickId;
}

inline const glm::mat4& Mat() { return sMatStack.back(); }

inline glm::vec3 XformPoint(const glm::vec3& p) {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

inline glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
}

inline glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    return glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f
                                        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
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
    PickGroup() : owned(sPickIdOverride == 0) {
        if (owned) { sPickIdOverride = ++sNextPickId; sLastPickId = sPickIdOverride; }
    }
    ~PickGroup() { if (owned) sPickIdOverride = 0; }
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
            glm::normalize(nmat * glm::vec3(v.normal))
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

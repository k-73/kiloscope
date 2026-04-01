#pragma once
// Internal header — shared state between Draw.cpp, DrawPrimitives.cpp, DrawGlobe.cpp.
// NOT part of public API. Do not include from outside src/Render/.

#include "Render/Draw.hpp"
#include "Render/Frame.hpp"
#include "Render/Camera.hpp"
#include "Render/Geo.hpp"
#include "Render/Fbo.hpp"
#include "Render/Shader.hpp"
#include <GL/glew.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Kilo::Render {

// ── types ────────────────────────────────────────────────────────────

struct MeshVert  { glm::vec3 pos, normal; glm::vec2 uv{0.f, 0.f}; };
struct LineVert  { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; uint32_t pickId; float width; };
struct PointVert { glm::vec3 pos; glm::vec4 color; uint32_t pickId; };
struct TextEntry {
    glm::vec3 worldPos; glm::vec4 color; std::string text;
    bool centered = false;
    glm::vec2 screenPos{-1.f, -1.f};  // if >= 0: use directly instead of projecting worldPos
};

// GPU-resident mesh (uploaded once, drawn with per-instance model matrix)
struct GpuMesh {
    GLuint  vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
};

// Cached indexed mesh (generated once, lazily uploaded to GPU)
struct IndexedMesh {
    std::vector<glm::vec3> pos, nrm;
    std::vector<glm::vec2> uv;
    std::vector<std::array<int, 3>> tri;
    float   boundingRadius = 0.f;
    GpuMesh gpu;
};

IndexedMesh& GetUnitSphere(int seg);
IndexedMesh& GetUnitBox();
IndexedMesh& GetUnitCylinder(int seg);
IndexedMesh& GetUnitCone(int seg);
IndexedMesh& GetUnitCapsule(int seg);
IndexedMesh& GetUnitDisk(int seg);

struct MeshDraw {
    GLsizei offset, count;
    glm::vec4 color;
    int shadingMode;          // 0=lit, 1=unlit, 2=emissive, 3=glow
    uint32_t pickId;
    const GpuMesh* gpuMesh = nullptr;   // non-null → GPU-side indexed draw
    glm::mat4 model{1.f};
    glm::mat3 normalMat{1.f};
    float worldRadius = 1.f;  // bounding sphere radius in world space (for frustum culling)
    bool twoSided = false;
};

// ── Frustum culling ─────────────────────────────────────────────────

struct ViewFrustum {
    glm::vec4 planes[6];  // left, right, bottom, top, near, far
};

inline ViewFrustum ExtractFrustum(const glm::mat4& vp) {
    ViewFrustum f;
    for (int i = 0; i < 3; ++i) {
        f.planes[i*2+0] = glm::vec4(vp[0][3]+vp[0][i], vp[1][3]+vp[1][i],
                                     vp[2][3]+vp[2][i], vp[3][3]+vp[3][i]);
        f.planes[i*2+1] = glm::vec4(vp[0][3]-vp[0][i], vp[1][3]-vp[1][i],
                                     vp[2][3]-vp[2][i], vp[3][3]-vp[3][i]);
    }
    for (auto& p : f.planes) p /= glm::length(glm::vec3(p));
    return f;
}

inline bool InsideFrustum(const ViewFrustum& f, const glm::vec3& center, float radius) {
    for (int i = 0; i < 6; ++i)
        if (glm::dot(glm::vec3(f.planes[i]), center) + f.planes[i].w < -radius)
            return false;
    return true;
}

// ── FBO types ────────────────────────────────────────────────────────

struct PickFbo {
    GLuint fbo = 0, color = 0, depth = 0;
    GLuint pbo[2] = {};         // double-buffered PBOs for async readback
    GLsync   fence[2] = {};       // fence per PBO — signals when readback is complete
    uint32_t lastPickId = 0;      // cached result (reused when GPU not ready)
    int      pboIdx = 0;          // current write PBO
    bool     pboReady = false;    // first frame: no read yet
    int w = 0, h = 0;
    void Resize(int nw, int nh);
    void Bind();
    void Clear(int cursorX, int cursorY, int radius);
    void BeginAsyncRead(int screenX, int screenY);   // non-blocking: start read into PBO
    uint32_t FinishAsyncRead();                       // non-blocking: read previous frame's PBO
    void Destroy();
    ~PickFbo() { Destroy(); }
    PickFbo() = default;
    PickFbo(const PickFbo&) = delete;
    PickFbo& operator=(const PickFbo&) = delete;
    PickFbo(PickFbo&& o) noexcept
        : fbo(o.fbo), color(o.color), depth(o.depth),
          lastPickId(o.lastPickId), pboIdx(o.pboIdx), pboReady(o.pboReady),
          w(o.w), h(o.h) {
        pbo[0] = o.pbo[0]; pbo[1] = o.pbo[1];
        fence[0] = o.fence[0]; fence[1] = o.fence[1];
        o.fbo = o.color = o.depth = 0;
        o.pbo[0] = o.pbo[1] = 0;
        o.fence[0] = o.fence[1] = nullptr;
        o.w = o.h = 0; o.lastPickId = 0; o.pboReady = false;
    }
    PickFbo& operator=(PickFbo&& o) noexcept {
        if (this != &o) { Destroy(); new (this) PickFbo(std::move(o)); }
        return *this;
    }
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
    Camera cam; Environment env; GridConfig gridCfg; GlobeConfig globeCfg;
    GeoRef geoRef;
    bool flyLocked = false;         // per-scene fly-mode cursor lock

    // Per-scene cached transforms (snapshot from last Begin/End)
    glm::mat4  cachedViewProj{1.f};
    glm::mat4  cachedInvViewProj{1.f};
    glm::dvec3 cachedCamPosD{0.0};
    float      cachedVpCx = 0, cachedVpCy = 0, cachedVpW = 1, cachedVpH = 1;

    // Mesh batching
    std::vector<MeshDraw> drawList;
    std::vector<MeshVert> vboAccum;

    // Line / point / text batching
    std::vector<LineVert>  lineBatch;
    std::vector<PointVert> pointBatch;
    std::vector<TextEntry> textBatch;
    float pointSize  = 4.f;

    // Coordinate frame convention (maps user axes → internal axes)
    glm::mat3 frameMat{1.f};

    // Transform stack
    std::vector<glm::mat4> matStack = {glm::mat4(1.f)};

    // Lights
    int numPointLights = 0;
    PointLightInfo pointLights[kMaxPointLights]{};

    // Pick state
    uint32_t hoveredPickId  = 0;
    uint32_t nextPickId     = 0;
    uint32_t activePickId     = 0;
    uint32_t pickIdOverride = 0;
    bool     pickEnabled    = true;
    bool     pickConsumed   = false;  // set by Clicked() to block End() overwrite

    // Drag state (per mouse button)
    uint32_t dragPickId[kButtonCount] = {};

    // Emissive one-shot flags
    bool  emissive       = false;
    bool  glow           = false;
    float glowRadius = 0.f;

    // Per-draw transient
    glm::vec4 currentColor{};
    int       currentShadingMode = 0;
    bool      twoSided           = false;

    // Frame stats & guard
    Stats stats{};
    bool  meshFrameReady = false;
};

struct FrameState {
    SceneData* scene{};
    float cx{}, cy{};       // viewport origin (screen coords)
    float w{}, h{};          // viewport size
    bool hovered{}, fly{};
    bool insideBeginEnd{};   // true between Begin() and End()
};

// ── shared state (GPU resources + per-frame derived) ─────────────────

inline Shader sMeshShader, sLineShader, sGridShader, sPointShader;
inline Shader sPickMeshShader, sPickLineShader, sPickPointShader;
inline GLuint sMeshVao = 0, sMeshVbo = 0;
inline GLuint sLineVao = 0, sLineVbo = 0;
inline GLuint sGridVao = 0;
inline GLuint sPointVao = 0, sPointVbo = 0;
inline GLsizeiptr sMeshVboCap = 0, sLineVboCap = 0, sPointVboCap = 0;

// Upload to VBO: always orphan to avoid GPU pipeline stalls.
// glNamedBufferData with nullptr discards old contents — driver can return a new
// allocation instead of waiting for the GPU to finish reading the previous frame's data.
inline void UploadVbo(GLuint vbo, GLsizeiptr& cap, const void* data, GLsizeiptr size) {
    if (size > cap) cap = size + size / 4;
    glNamedBufferData(vbo, cap, nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferSubData(vbo, 0, size, data);
}

inline glm::mat4 sView, sProj, sViewProj, sInvViewProj;
inline glm::dvec3 sCamPosD;          // double — for Globe/Geo precision
inline glm::vec3 sCamPos, sLightDir;
inline ViewFrustum sFrustum;
inline float sFarPlane = 10000.f;
inline int   sMsaaSamples = 8;
inline int sVpW = 1, sVpH = 1;

inline std::vector<MeshVert> sMeshScratch;
inline std::vector<MeshVert> sIndexedScratch;

inline std::unordered_map<uint32_t, std::unique_ptr<SceneData>> sScenes;
inline FrameState sFrame;

// ── scene accessors ──────────────────────────────────────────────────

inline SceneData& ctx() {
    assert(sFrame.scene && "Draw call outside Begin()/End()");
    return *sFrame.scene;
}

// ── inline helpers ───────────────────────────────────────────────────

// Normal matrix with singularity guard (prevents NaN from zero-scale transforms).
inline glm::mat3 NormalMatrix(const glm::mat4& model) {
    auto m = glm::mat3(model);
    float det = glm::determinant(m);
    return std::abs(det) > 1e-30f ? glm::transpose(glm::inverse(m)) : glm::mat3(1.f);
}

inline uint32_t HashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

inline SceneData& GetScene(uint32_t id) {
    auto& s = sScenes[id]; if (!s) s = std::make_unique<SceneData>(); return *s;
}
inline SceneData& GetScene(const char* name) { return GetScene(HashName(name)); }

inline uint32_t AllocPickId() {
    return ctx().pickIdOverride ? ctx().pickIdOverride : ++ctx().nextPickId;
}

inline const glm::mat4& Mat() { return ctx().matStack.back(); }

// Transform to camera-relative world space (double subtraction for precision at altitude)
inline glm::vec3 XformPoint(const glm::vec3& p) {
    auto world = Mat() * glm::vec4(p, 1.f);
    return glm::vec3(glm::dvec3(world) - sCamPosD);
}

inline glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
}

inline glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    auto ref = std::abs(n.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    return glm::normalize(glm::cross(n, ref));
}

// Build rotation matrix that maps +Z to dir (stable for all directions, no singularity)
inline glm::mat4 AxisRotation(const glm::vec3& dir) {
    auto z = glm::normalize(dir);
    auto ref = std::abs(z.z) < 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    auto x = glm::normalize(glm::cross(ref, z));
    auto y = glm::cross(z, x);
    return {glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0), glm::vec4(0, 0, 0, 1)};
}

inline glm::mat4 ZAlign(const glm::vec3& a, const glm::vec3& b) {
    return glm::translate(glm::mat4(1.f), (a + b) * 0.5f) * AxisRotation(b - a);
}

inline glm::mat4 AxisTransform(const glm::vec3& center, const glm::vec3& axis) {
    return glm::translate(glm::mat4(1.f), center) * AxisRotation(axis);
}

// ── RAII pick group ──────────────────────────────────────────────────

// PickGroup is an alias for the public Group (same RAII pick-ID grouping)
using PickGroup = Group;

// ── function declarations (defined in Draw.cpp, called by DrawPrimitives.cpp)
void SetMeshUniforms(const glm::vec4& color, bool unlit = false);
void UploadMesh(const std::vector<MeshVert>& v);
void UploadGpuDraw(IndexedMesh& mesh, const glm::mat4& model);
void ShutdownModels();  // defined in Model.cpp
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
    auto nmat = NormalMatrix(xform);
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

} // namespace Kilo::Render

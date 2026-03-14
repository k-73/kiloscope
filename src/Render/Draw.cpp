#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include "Render/Fbo.hpp"
#include "Render/Shader.hpp"
#include <GL/glew.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <generator/SphereMesh.hpp>
#include <generator/BoxMesh.hpp>
#include <generator/CappedCylinderMesh.hpp>
#include <generator/CappedConeMesh.hpp>
#include <generator/CapsuleMesh.hpp>
#include <generator/TorusMesh.hpp>
#include <generator/DiskMesh.hpp>
#include <cassert>
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

// ── pick FBO ─────────────────────────────────────────────────────────

struct PickFbo {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;

    void Resize(int nw, int nh) {
        if (nw == w && nh == h) return;
        Destroy();
        w = nw; h = nh;
        glCreateFramebuffers(1, &fbo);
        glCreateTextures(GL_TEXTURE_2D, 1, &color);
        glTextureStorage2D(color, 1, GL_R32UI, w, h);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
        glCreateRenderbuffers(1, &depth);
        glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT32F, w, h);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }

    void Bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, w, h); }

    void Clear() {
        Bind();
        GLuint zero = 0;
        glClearBufferuiv(GL_COLOR, 0, &zero);
        float one = 1.f;
        glClearBufferfv(GL_DEPTH, 0, &one);
    }

    uint32_t ReadPixel(int x, int y) const {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        uint32_t id = 0;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);
        return id;
    }

    void Destroy() {
        if (fbo)   { glDeleteFramebuffers(1, &fbo);   fbo = 0; }
        if (color) { glDeleteTextures(1, &color);      color = 0; }
        if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
        w = h = 0;
    }

    ~PickFbo() { Destroy(); }
    PickFbo() = default;
    PickFbo(const PickFbo&) = delete;
    PickFbo& operator=(const PickFbo&) = delete;
};

// ── shared GPU resources ─────────────────────────────────────────────

static Shader sMeshShader, sLineShader, sGridShader, sPointShader;
static Shader sPickMeshShader, sPickLineShader, sPickPointShader;
static GLuint sMeshVao = 0, sMeshVbo = 0;
static GLuint sLineVao = 0, sLineVbo = 0;
static GLuint sGridVao = 0;
static GLuint sPointVao = 0, sPointVbo = 0;

// ── per-frame render state ───────────────────────────────────────────

static glm::mat4 sView, sProj, sViewProj;
static glm::vec3 sCamPos, sLightDir;
static int sVpW = 1, sVpH = 1;
static std::vector<LineVert>  sLineBatch;
static std::vector<PointVert> sPointBatch;
static std::vector<TextEntry> sTextBatch;
static float sLineWidth  = 2.5f;
static float sPointSize  = 4.f;
static std::vector<glm::mat4> sMatStack = {glm::mat4(1.f)};
static std::vector<MeshVert>  sMeshScratch;
static std::vector<MeshVert>  sIndexedScratch;

// ── per-scene state ──────────────────────────────────────────────────

struct SceneData { Fbo fbo; PickFbo pickFbo; Camera cam; Environment env; GridConfig gridCfg; };

static std::unordered_map<uint32_t, std::unique_ptr<SceneData>> sScenes;
static struct { SceneData* scene{}; float cx{}, cy{}, w{}, h{}; bool hovered{}; } sFrame;
static Environment* sEnv = nullptr;

static uint32_t HashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

// ── GPU pick state ───────────────────────────────────────────────────

static uint32_t sNextPickId     = 0;
static uint32_t sLastPickId     = 0;
static uint32_t sPickIdOverride = 0;
static uint32_t sHoveredPickId  = 0;
static bool     sPickEnabled    = true;

static uint32_t AllocPickId() {
    return sPickIdOverride ? sPickIdOverride : ++sNextPickId;
}

// RAII: groups all sub-primitives under a single pick ID
struct PickGroup {
    bool owned;
    PickGroup() : owned(sPickIdOverride == 0) {
        if (owned) { sPickIdOverride = ++sNextPickId; sLastPickId = sPickIdOverride; }
    }
    ~PickGroup() { if (owned) sPickIdOverride = 0; }
};

// ── transform helpers ────────────────────────────────────────────────

static const glm::mat4& Mat() { return sMatStack.back(); }

static glm::vec3 XformPoint(const glm::vec3& p) {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

static glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
}

// ── geometry helpers ─────────────────────────────────────────────────

static glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    return glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f
                                        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
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

// ── GPU helpers ──────────────────────────────────────────────────────

static void SetupVao(GLuint vao, GLuint vbo, GLsizei stride,
                     std::initializer_list<std::pair<GLuint, std::pair<GLint, GLuint>>> attrs) {
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
    for (auto& [idx, spec] : attrs) {
        glEnableVertexArrayAttrib(vao, idx);
        glVertexArrayAttribFormat(vao, idx, spec.first, GL_FLOAT, GL_FALSE, spec.second);
        glVertexArrayAttribBinding(vao, idx, 0);
    }
}

static bool sMeshFrameReady = false;
static bool sPickMeshReady  = false;

static void SetMeshFrameUniforms() {
    if (sMeshFrameReady) return;
    sMeshShader.Use();
    sMeshShader.Set("uViewProj", sViewProj);
    sMeshShader.Set("uLightDir", sLightDir);
    sMeshShader.Set("uCamPos", sCamPos);
    sMeshShader.Set("uBgColor", sEnv->bgColor);
    sMeshShader.Set("uAmbient", sEnv->ambient);
    sMeshShader.Set("uDiffuse", sEnv->diffuse);
    sMeshShader.Set("uRoughness", sEnv->roughness);
    sMeshShader.Set("uSpecular", sEnv->specular);
    sMeshShader.Set("uFresnel", sEnv->fresnel);
    sMeshShader.Set("uFogDensity", sEnv->fogDensity);
    sMeshFrameReady = true;
}

static void SetMeshUniforms(const glm::vec4& color, bool unlit = false) {
    SetMeshFrameUniforms();
    sMeshShader.Use();
    sMeshShader.Set("uColor", color);
    sMeshShader.Set("uUnlit", unlit ? 1 : 0);
}

// ── pick pass helpers ────────────────────────────────────────────────

static void BeginPickPass() {
    sFrame.scene->pickFbo.Bind();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void EndPickPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, sFrame.scene->fbo.Handle());
    glViewport(0, 0, sVpW, sVpH);
}

static void UploadMesh(const std::vector<MeshVert>& v) {
    auto count = static_cast<GLsizei>(v.size());
    glNamedBufferData(sMeshVbo, GLsizeiptr(count * sizeof(MeshVert)),
                      v.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(sMeshVao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);

    if (sPickEnabled && sLastPickId) {
        BeginPickPass();
        sPickMeshShader.Use();
        if (!sPickMeshReady) {
            sPickMeshShader.Set("uViewProj", sViewProj);
            sPickMeshReady = true;
        }
        sPickMeshShader.Set("uPickId", sLastPickId);
        glEnable(GL_CULL_FACE);
        glBindVertexArray(sMeshVao);
        glDrawArrays(GL_TRIANGLES, 0, count);
        glBindVertexArray(0);
        EndPickPass();
    }
}

template <typename MeshT>
static void AppendMesh(std::vector<MeshVert>& out, const MeshT& mesh,
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

// ── line batching ────────────────────────────────────────────────────

static void FlushLines() {
    if (sLineBatch.empty()) return;
    auto count = static_cast<GLsizei>(sLineBatch.size());

    glNamedBufferData(sLineVbo, GLsizeiptr(count * sizeof(LineVert)),
                      sLineBatch.data(), GL_DYNAMIC_DRAW);

    sLineShader.Use();
    sLineShader.Set("uView", sView);
    sLineShader.Set("uProj", sProj);
    sLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    sLineShader.Set("uLineWidth", sLineWidth);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sLineVao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    if (sPickEnabled) {
        BeginPickPass();
        sPickLineShader.Use();
        sPickLineShader.Set("uView", sView);
        sPickLineShader.Set("uProj", sProj);
        sPickLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        sPickLineShader.Set("uLineWidth", sLineWidth);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(sLineVao);
        glDrawArrays(GL_TRIANGLES, 0, count);
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        EndPickPass();
    }

    sLineBatch.clear();
}

static void BatchLineGradient(const glm::vec3& a, const glm::vec3& b,
                               const glm::vec4& ca, const glm::vec4& cb, float width) {
    if (!sLineBatch.empty() && width != sLineWidth)
        FlushLines();
    sLineWidth = width;
    uint32_t pid = sLastPickId;
    sLineBatch.push_back({a, b, {-1, 0}, ca, pid});
    sLineBatch.push_back({a, b, { 1, 0}, ca, pid});
    sLineBatch.push_back({a, b, { 1, 1}, cb, pid});
    sLineBatch.push_back({a, b, {-1, 0}, ca, pid});
    sLineBatch.push_back({a, b, { 1, 1}, cb, pid});
    sLineBatch.push_back({a, b, {-1, 1}, cb, pid});
}

static void BatchLine(const glm::vec3& a, const glm::vec3& b,
                      const glm::vec4& color, float width) {
    BatchLineGradient(a, b, color, color, width);
}

// ── point batching ───────────────────────────────────────────────────

static void FlushPoints() {
    if (sPointBatch.empty()) return;
    auto count = static_cast<GLsizei>(sPointBatch.size());

    glNamedBufferData(sPointVbo, GLsizeiptr(count * sizeof(PointVert)),
                      sPointBatch.data(), GL_DYNAMIC_DRAW);

    sPointShader.Use();
    sPointShader.Set("uView", sView);
    sPointShader.Set("uProj", sProj);
    sPointShader.Set("uPointSize", sPointSize);
    sPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sPointVao);
    glDrawArrays(GL_POINTS, 0, count);
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    if (sPickEnabled) {
        BeginPickPass();
        sPickPointShader.Use();
        sPickPointShader.Set("uView", sView);
        sPickPointShader.Set("uProj", sProj);
        sPickPointShader.Set("uPointSize", sPointSize);
        sPickPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(sPointVao);
        glDrawArrays(GL_POINTS, 0, count);
        glBindVertexArray(0);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_CULL_FACE);
        EndPickPass();
    }

    sPointBatch.clear();
}

// ── projection helpers ───────────────────────────────────────────────

glm::vec2 WorldToScreen(const glm::vec3& worldPos) {
    glm::vec4 clip = sViewProj * glm::vec4(worldPos, 1.f);
    if (clip.w <= 0.f) return {-1.f, -1.f};
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return {
        sFrame.cx + (ndc.x * 0.5f + 0.5f) * sFrame.w,
        sFrame.cy + (1.f - (ndc.y * 0.5f + 0.5f)) * sFrame.h
    };
}

// ── interaction ──────────────────────────────────────────────────────

bool EventState::Clicked(int button) const {
    return hovered_ && ImGui::IsMouseClicked(button);
}

EventState Event() {
    EventState state;
    state.hovered_ = sFrame.hovered && sLastPickId != 0
                  && sLastPickId == sHoveredPickId;
    return state;
}

// ── text overlay ─────────────────────────────────────────────────────

static void FlushText() {
    if (sTextBatch.empty()) return;
    auto* dl = ImGui::GetWindowDrawList();
    ImGui::PushClipRect({sFrame.cx, sFrame.cy},
                        {sFrame.cx + sFrame.w, sFrame.cy + sFrame.h}, true);
    for (auto& e : sTextBatch) {
        auto screen = WorldToScreen(e.worldPos);
        if (screen.x < 0.f) continue;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(
            {e.color.r, e.color.g, e.color.b, e.color.a});
        dl->AddText({screen.x, screen.y}, col, e.text.c_str());
    }
    ImGui::PopClipRect();
    sTextBatch.clear();
}

// ── Jacobi eigensolver for 3x3 symmetric ─────────────────────────────

static void Eigen3(const glm::mat3& A, glm::vec3& eigenvalues, glm::mat3& eigenvectors) {
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

// ── scene viewport ───────────────────────────────────────────────────

void Init(const std::string& dir) {
    sMeshShader      = Shader(dir + "/Basic.vert",     dir + "/Basic.frag");
    sLineShader      = Shader(dir + "/Line.vert",      dir + "/Line.frag");
    sGridShader      = Shader(dir + "/Grid.vert",      dir + "/Grid.frag");
    sPointShader     = Shader(dir + "/Point.vert",     dir + "/Point.frag");
    sPickMeshShader  = Shader(dir + "/Pick.vert",      dir + "/Pick.frag");
    sPickLineShader  = Shader(dir + "/PickLine.vert",  dir + "/PickLine.frag");
    sPickPointShader = Shader(dir + "/PickPoint.vert", dir + "/PickPoint.frag");

    glCreateVertexArrays(1, &sMeshVao);
    glCreateBuffers(1, &sMeshVbo);
    SetupVao(sMeshVao, sMeshVbo, sizeof(MeshVert), {
        {0, {3, offsetof(MeshVert, pos)}},
        {1, {3, offsetof(MeshVert, normal)}}});

    glCreateVertexArrays(1, &sLineVao);
    glCreateBuffers(1, &sLineVbo);
    SetupVao(sLineVao, sLineVbo, sizeof(LineVert), {
        {0, {3, offsetof(LineVert, pos)}},
        {1, {3, offsetof(LineVert, otherEnd)}},
        {2, {2, offsetof(LineVert, expand)}},
        {3, {4, offsetof(LineVert, color)}}});
    glEnableVertexArrayAttrib(sLineVao, 4);
    glVertexArrayAttribIFormat(sLineVao, 4, 1, GL_UNSIGNED_INT, offsetof(LineVert, pickId));
    glVertexArrayAttribBinding(sLineVao, 4, 0);

    glCreateVertexArrays(1, &sGridVao);

    glCreateVertexArrays(1, &sPointVao);
    glCreateBuffers(1, &sPointVbo);
    SetupVao(sPointVao, sPointVbo, sizeof(PointVert), {
        {0, {3, offsetof(PointVert, pos)}},
        {1, {4, offsetof(PointVert, color)}}});
    glEnableVertexArrayAttrib(sPointVao, 2);
    glVertexArrayAttribIFormat(sPointVao, 2, 1, GL_UNSIGNED_INT, offsetof(PointVert, pickId));
    glVertexArrayAttribBinding(sPointVao, 2, 0);
}

static SceneData& GetScene(uint32_t id) {
    auto& s = sScenes[id];
    if (!s) s = std::make_unique<SceneData>();
    return *s;
}

static SceneData& GetScene(const char* name) { return GetScene(HashName(name)); }

Camera& GetCamera() {
    assert(sFrame.scene && "GetCamera requires active scene");
    return sFrame.scene->cam;
}

Camera& GetCamera(const char* name) { return GetScene(name).cam; }

Environment& GetEnvironment() {
    assert(sFrame.scene && "GetEnvironment requires active scene");
    return sFrame.scene->env;
}

Environment& GetEnvironment(const char* name) { return GetScene(name).env; }

void Begin(const char* name, const ViewportConfig& cfg) {
    auto& scene = sScenes[HashName(name)];
    if (!scene) scene = std::make_unique<SceneData>();

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(cfg.width  > 0 ? cfg.width  : avail.x));
    int h = std::max(1, static_cast<int>(cfg.height > 0 ? cfg.height : avail.y));
    scene->fbo.Resize(w, h, 16);
    scene->pickFbo.Resize(w, h);

    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    auto& io  = ImGui::GetIO();
    auto& cam = scene->cam;
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    if (hovered && io.MouseWheel != 0)
        cam.Zoom(io.MouseWheel);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    sFrame = { scene.get(), cursor.x, cursor.y, size.x, size.y, hovered };
    sEnv = &scene->env;

    const auto& bg = sEnv->bgColor;
    scene->fbo.Bind(bg.r, bg.g, bg.b);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    float aspect = static_cast<float>(w) / std::max(1, h);
    sView     = cam.View();
    sProj     = cam.Projection(aspect);
    sViewProj = sProj * sView;
    sCamPos   = cam.Position();
    sLightDir = glm::normalize(sEnv->lightDir);
    sVpW = w; sVpH = h;

    // Init pick state
    scene->pickFbo.Clear();
    glBindFramebuffer(GL_FRAMEBUFFER, scene->fbo.Handle());
    glViewport(0, 0, w, h);

    sNextPickId = 0;
    sLastPickId = 0;
    sPickIdOverride = 0;
    sPickEnabled = true;
    sMeshFrameReady = false;
    sPickMeshReady = false;
    sLineBatch.clear();
    sPointBatch.clear();
    sTextBatch.clear();
    sMatStack.resize(1);
    sMatStack[0] = glm::mat4(1.f);
}

static void DrawSun() {
    sPickEnabled = false;
    float r = sEnv->sunRadius;
    glm::vec3 sunPos = glm::normalize(sEnv->lightDir) * sEnv->sunDistance;
    glm::vec3 facing = glm::normalize(sCamPos - sunPos);

    PushMatrix();
    ResetMatrix();

    SetMeshUniforms({1.f, .98f, .85f, 1.f}, true);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::SphereMesh(r, 16, 8),
               glm::translate(glm::mat4(1.f), sunPos));
    UploadMesh(sMeshScratch);

    Circle(sunPos, facing, r * 1.8f, {1.f, .95f, .7f, .45f}, 32, 3.f);
    Circle(sunPos, facing, r * 3.f,  {1.f, .9f,  .5f, .18f}, 32, 2.f);
    Circle(sunPos, facing, r * 5.f,  {1.f, .85f, .4f, .07f}, 32, 1.5f);
    Line({0, 0, 0}, sunPos, {1.f, .95f, .7f, .12f}, 1.f);

    PopMatrix();
    sPickEnabled = true;
}

static void DrawGrid(const GridConfig& cfg, float camDist) {
    sGridShader.Use();
    sGridShader.Set("uView", sView);
    sGridShader.Set("uProj", sProj);
    sGridShader.Set("uViewProj", sViewProj);
    sGridShader.Set("uCamPos", sCamPos);
    sGridShader.Set("uCamDist", camDist);
    sGridShader.Set("uScaleFine",   cfg.scaleFine);
    sGridShader.Set("uScaleMedium", cfg.scaleMedium);
    sGridShader.Set("uScaleCoarse", cfg.scaleCoarse);
    sGridShader.Set("uColorFine",   cfg.colorFine);
    sGridShader.Set("uColorMedium", cfg.colorMedium);
    sGridShader.Set("uColorCoarse", cfg.colorCoarse);
    sGridShader.Set("uAlphaFine",   cfg.alphaFine);
    sGridShader.Set("uAlphaMedium", cfg.alphaMedium);
    sGridShader.Set("uAlphaCoarse", cfg.alphaCoarse);
    sGridShader.Set("uAxisXColor",  cfg.axisXColor);
    sGridShader.Set("uAxisYColor",  cfg.axisYColor);
    sGridShader.Set("uAxisThickness", cfg.axisThickness);
    sGridShader.Set("uAxisAlpha",   cfg.axisAlpha);
    sGridShader.Set("uAxisScaleWithCam", cfg.axisScaleWithCam ? 1 : 0);
    sGridShader.Set("uFadeStart",   cfg.fadeStart);
    sGridShader.Set("uFadeEnd",     cfg.fadeEnd);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sGridVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_CULL_FACE);
}

void Grid() {
    assert(sFrame.scene && "Grid requires active scene");
    sFrame.scene->gridCfg.enabled = true;
}

void Grid(const GridConfig& cfg) {
    assert(sFrame.scene && "Grid requires active scene");
    sFrame.scene->gridCfg = cfg;
}

GridConfig& GetGrid() {
    assert(sFrame.scene && "GetGrid requires active scene");
    return sFrame.scene->gridCfg;
}

GridConfig& GetGrid(const char* name) { return GetScene(name).gridCfg; }

void End() {
    if (sEnv->showSun) DrawSun();
    FlushPoints();
    FlushLines();
    auto& g = sFrame.scene->gridCfg;
    if (g.enabled)
        DrawGrid(g, sFrame.scene->cam.Distance());

    // Read pick buffer at mouse position
    if (sFrame.hovered) {
        auto& io = ImGui::GetIO();
        int mx = static_cast<int>(io.MousePos.x - sFrame.cx);
        int my = static_cast<int>(sFrame.h - 1.f - (io.MousePos.y - sFrame.cy));
        sHoveredPickId = sFrame.scene->pickFbo.ReadPixel(mx, my);
    } else {
        sHoveredPickId = 0;
    }

    sFrame.scene->fbo.Resolve();
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(sFrame.scene->fbo.Texture())),
                 {sFrame.w, sFrame.h}, {0, 1}, {1, 0});
    FlushText();
}

// ── transform stack ──────────────────────────────────────────────────

void PushMatrix()  { sMatStack.push_back(Mat()); }
void PopMatrix()   { if (sMatStack.size() > 1) sMatStack.pop_back(); }
void ResetMatrix() { sMatStack.back() = glm::mat4(1.f); }

void SetMatrix(const glm::mat4& m)  { sMatStack.back() = m; }
void Transform(const glm::mat4& m)  { sMatStack.back() *= m; }

void Translate(const glm::vec3& offset) {
    sMatStack.back() = glm::translate(sMatStack.back(), offset);
}
void Translate(float x, float y, float z) { Translate({x, y, z}); }

void Rotate(float angleDeg, const glm::vec3& axis) {
    sMatStack.back() = glm::rotate(sMatStack.back(), glm::radians(angleDeg), axis);
}
void Rotate(const glm::quat& q) { sMatStack.back() *= glm::mat4_cast(q); }
void RotateX(float deg) { Rotate(deg, {1, 0, 0}); }
void RotateY(float deg) { Rotate(deg, {0, 1, 0}); }
void RotateZ(float deg) { Rotate(deg, {0, 0, 1}); }

void Scale(const glm::vec3& s) {
    sMatStack.back() = glm::scale(sMatStack.back(), s);
}
void Scale(float s) { Scale({s, s, s}); }

// ── lines ────────────────────────────────────────────────────────────

void Line(const glm::vec3& a, const glm::vec3& b,
          const glm::vec4& color, float width) {
    sLastPickId = AllocPickId();
    BatchLine(XformPoint(a), XformPoint(b), color, width);
}

void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width, bool closed) {
    if (count < 2) return;
    sLastPickId = AllocPickId();
    auto first = XformPoint(points[0]);
    auto prev = first;
    for (int i = 1; i < count; ++i) {
        auto cur = XformPoint(points[i]);
        BatchLine(prev, cur, color, width);
        prev = cur;
    }
    if (closed && count > 2) BatchLine(prev, first, color, width);
}

void Path(const glm::vec3* points, const glm::vec4* colors,
          int count, float width, bool closed) {
    if (count < 2) return;
    sLastPickId = AllocPickId();
    auto first = XformPoint(points[0]);
    auto prev = first;
    for (int i = 1; i < count; ++i) {
        auto cur = XformPoint(points[i]);
        BatchLineGradient(prev, cur, colors[i - 1], colors[i], width);
        prev = cur;
    }
    if (closed && count > 2) BatchLineGradient(prev, first, colors[count - 1], colors[0], width);
}

void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg, float width) {
    sLastPickId = AllocPickId();
    auto tc = XformPoint(center);
    auto ta = glm::normalize(XformDir(axis));
    auto ts = glm::normalize(XformDir(startDir));
    float step = glm::radians(angleDeg) / static_cast<float>(seg);
    auto prev = tc + ts * radius;
    for (int i = 1; i <= seg; ++i) {
        auto cur = tc + glm::vec3(glm::rotate(glm::mat4(1), step * i, ta)
                                  * glm::vec4(ts * radius, 0));
        BatchLine(prev, cur, color, width);
        prev = cur;
    }
}

void Circle(const glm::vec3& center, const glm::vec3& axis,
            float radius, const glm::vec4& color, int seg, float width) {
    Arc(center, axis, Perpendicular(axis), radius, 360.f, color, seg, width);
}

void Spline(const glm::vec3* cp, int count,
            const glm::vec4& color, int segments, float width) {
    if (count < 2) return;
    if (count == 2) { Line(cp[0], cp[1], color, width); return; }

    sLastPickId = AllocPickId();
    auto catmullRom = [](const glm::vec3& p0, const glm::vec3& p1,
                         const glm::vec3& p2, const glm::vec3& p3, float t) {
        return 0.5f * ((2.f * p1) +
            (-p0 + p2) * t +
            (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
            (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
    };

    for (int i = 0; i < count - 1; ++i) {
        const auto& p0 = cp[std::max(0, i - 1)];
        const auto& p1 = cp[i];
        const auto& p2 = cp[std::min(count - 1, i + 1)];
        const auto& p3 = cp[std::min(count - 1, i + 2)];
        auto prev = XformPoint(p1);
        for (int s = 1; s <= segments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(segments);
            auto cur = XformPoint(catmullRom(p0, p1, p2, p3, t));
            BatchLine(prev, cur, color, width);
            prev = cur;
        }
    }
}

// ── points ───────────────────────────────────────────────────────────

void Points(const glm::vec3* positions, int count,
            const glm::vec4& color, float size) {
    sLastPickId = AllocPickId();
    if (!sPointBatch.empty() && size != sPointSize) FlushPoints();
    sPointSize = size;
    for (int i = 0; i < count; ++i)
        sPointBatch.push_back({XformPoint(positions[i]), color, sLastPickId});
}

void Points(const glm::vec3* positions, const glm::vec4* colors,
            int count, float size) {
    sLastPickId = AllocPickId();
    if (!sPointBatch.empty() && size != sPointSize) FlushPoints();
    sPointSize = size;
    for (int i = 0; i < count; ++i)
        sPointBatch.push_back({XformPoint(positions[i]), colors[i], sLastPickId});
}

// ── text ─────────────────────────────────────────────────────────────

void Text(const glm::vec3& pos, const glm::vec4& color, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sTextBatch.push_back({XformPoint(pos), color, buf});
}

// ── basic geometry ───────────────────────────────────────────────────

void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color) {
    sLastPickId = AllocPickId();
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto normal = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal}});
}

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    sLastPickId = AllocPickId();
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c), td = XformPoint(d);
    auto normal = glm::normalize(glm::cross(tb - ta, td - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal},
                {ta, normal}, {tc, normal}, {td, normal}});
}

void Plane(const glm::vec3& center, const glm::vec3& normal,
           const glm::vec2& halfSize, const glm::vec4& color) {
    auto n = glm::normalize(normal);
    auto u = Perpendicular(n);
    auto v = glm::cross(n, u);
    Quad(center + (-u * halfSize.x - v * halfSize.y),
         center + ( u * halfSize.x - v * halfSize.y),
         center + ( u * halfSize.x + v * halfSize.y),
         center + (-u * halfSize.x + v * halfSize.y), color);
}

// ── mesh primitives ──────────────────────────────────────────────────

void Sphere(const glm::vec3& center, float radius,
            const glm::vec4& color, int seg) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::SphereMesh(radius, seg, seg / 2),
               glm::translate(Mat(), center));
    UploadMesh(sMeshScratch);
}

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::BoxMesh({size.x, size.y, size.z}, {1, 1, 1}),
               glm::translate(Mat(), center));
    UploadMesh(sMeshScratch);
}

void Cube(const glm::vec3& center, float size, const glm::vec4& color) {
    Box(center, glm::vec3(size), color);
}

void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) return;
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CappedCylinderMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(a, b));
    UploadMesh(sMeshScratch);
}

void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(tip - base) * 0.5f;
    if (halfLen < 1e-6f) return;
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CappedConeMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(base, tip));
    UploadMesh(sMeshScratch);
}

void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; }
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CapsuleMesh(radius, halfLen, seg, 1, seg / 2),
               Mat() * ZAlign(a, b));
    UploadMesh(sMeshScratch);
}

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(sMeshScratch);
}

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::DiskMesh(radius, 0.0, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(sMeshScratch);
}

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(sMeshScratch);
}

// ── custom mesh ──────────────────────────────────────────────────────

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          const uint32_t* indices, int indexCount, const glm::vec4& color) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    auto& m = Mat();
    auto nmat = glm::transpose(glm::inverse(glm::mat3(m)));
    sMeshScratch.clear();
    sMeshScratch.reserve(indexCount);
    for (int i = 0; i < indexCount; ++i) {
        uint32_t idx = indices[i];
        sMeshScratch.push_back({glm::vec3(m * glm::vec4(verts[idx], 1.f)),
                                 glm::normalize(nmat * normals[idx])});
    }
    UploadMesh(sMeshScratch);
}

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          int vertCount, const glm::vec4& color) {
    sLastPickId = AllocPickId();
    SetMeshUniforms(color);
    auto& m = Mat();
    auto nmat = glm::transpose(glm::inverse(glm::mat3(m)));
    sMeshScratch.clear();
    sMeshScratch.reserve(vertCount);
    for (int i = 0; i < vertCount; ++i) {
        sMeshScratch.push_back({glm::vec3(m * glm::vec4(verts[i], 1.f)),
                                 glm::normalize(nmat * normals[i])});
    }
    UploadMesh(sMeshScratch);
}

// ── wireframe ────────────────────────────────────────────────────────

void WireBox(const glm::vec3& center, const glm::vec3& size,
             const glm::vec4& color, float width) {
    PickGroup pg;
    auto hs = size * 0.5f;
    glm::vec3 c[8] = {
        center + glm::vec3(-hs.x, -hs.y, -hs.z),
        center + glm::vec3( hs.x, -hs.y, -hs.z),
        center + glm::vec3( hs.x,  hs.y, -hs.z),
        center + glm::vec3(-hs.x,  hs.y, -hs.z),
        center + glm::vec3(-hs.x, -hs.y,  hs.z),
        center + glm::vec3( hs.x, -hs.y,  hs.z),
        center + glm::vec3( hs.x,  hs.y,  hs.z),
        center + glm::vec3(-hs.x,  hs.y,  hs.z),
    };
    for (int i = 0; i < 4; ++i) {
        Line(c[i], c[(i+1)%4], color, width);
        Line(c[i+4], c[(i+1)%4+4], color, width);
        Line(c[i], c[i+4], color, width);
    }
}

void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg, float width) {
    PickGroup pg;
    Circle(center, {1, 0, 0}, radius, color, seg, width);
    Circle(center, {0, 1, 0}, radius, color, seg, width);
    Circle(center, {0, 0, 1}, radius, color, seg, width);
}

void WireCylinder(const glm::vec3& a, const glm::vec3& b,
                  float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    PickGroup pg;
    auto dir  = axis / len;
    auto perp = Perpendicular(dir);
    auto side = glm::cross(dir, perp);

    Circle(a, dir, radius, color, seg, width);
    Circle(b, dir, radius, color, seg, width);

    for (int i = 0; i < 4; ++i) {
        float angle = glm::half_pi<float>() * static_cast<float>(i);
        auto d = perp * std::cos(angle) + side * std::sin(angle);
        Line(a + d * radius, b + d * radius, color, width);
    }
}

void WireCone(const glm::vec3& base, const glm::vec3& tip,
              float radius, const glm::vec4& color, int seg, float width) {
    auto axis = tip - base;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    PickGroup pg;
    auto dir  = axis / len;
    auto perp = Perpendicular(dir);
    auto side = glm::cross(dir, perp);

    Circle(base, dir, radius, color, seg, width);

    for (int i = 0; i < 4; ++i) {
        float angle = glm::half_pi<float>() * static_cast<float>(i);
        auto d = perp * std::cos(angle) + side * std::sin(angle);
        Line(base + d * radius, tip, color, width);
    }
}

void WireCapsule(const glm::vec3& a, const glm::vec3& b,
                 float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) { WireSphere((a + b) * 0.5f, radius, color, seg, width); return; }
    PickGroup pg;
    auto dir  = axis / len;
    auto perp = Perpendicular(dir);
    auto side = glm::cross(dir, perp);

    Circle(a, dir, radius, color, seg, width);
    Circle(b, dir, radius, color, seg, width);

    for (int i = 0; i < 4; ++i) {
        float angle = glm::half_pi<float>() * static_cast<float>(i);
        auto d = perp * std::cos(angle) + side * std::sin(angle);
        Line(a + d * radius, b + d * radius, color, width);
    }

    int halfSeg = std::max(4, seg / 2);
    Arc(a, perp, -dir, radius, 180.f, color, halfSeg, width);
    Arc(a, side, -dir, radius, 180.f, color, halfSeg, width);
    Arc(b, perp,  dir, radius, 180.f, color, halfSeg, width);
    Arc(b, side,  dir, radius, 180.f, color, halfSeg, width);
}

// ── composite ────────────────────────────────────────────────────────

void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR, float headR) {
    auto dir = to - from;
    float len = glm::length(dir);
    if (len < 1e-6f) return;

    sLastPickId = AllocPickId();
    float headLen = std::min(len * 0.25f, headR * 2.5f);
    auto shaftEnd = from + dir * ((len - headLen) / len);

    SetMeshUniforms(color);
    sMeshScratch.clear();
    float halfShaft = glm::length(shaftEnd - from) * 0.5f;
    float halfHead  = headLen * 0.5f;
    AppendMesh(sMeshScratch, generator::CappedCylinderMesh(shaftR, halfShaft, 24, 1, 1),
               Mat() * ZAlign(from, shaftEnd));
    AppendMesh(sMeshScratch, generator::CappedConeMesh(headR, halfHead, 24, 1, 1),
               Mat() * ZAlign(shaftEnd, to));
    UploadMesh(sMeshScratch);
}

void Axes(const glm::vec3& origin, float len) {
    PickGroup pg;
    float s = len * 0.025f, h = len * 0.07f;
    Arrow(origin, origin + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
}

void Frame(const glm::mat4& pose, float len) {
    PickGroup pg;
    PushMatrix();
    Transform(pose);
    Axes({0, 0, 0}, len);
    PopMatrix();
}

void Frame(const glm::vec3& pos, const glm::quat& orient, float len) {
    Frame(glm::translate(glm::mat4(1.f), pos) * glm::mat4_cast(orient), len);
}

void Point(const glm::vec3& pos, const glm::vec4& color, float size) {
    Sphere(pos, size, color, 8);
}

void Cross(const glm::vec3& pos, float size,
           const glm::vec4& color, float width) {
    PickGroup pg;
    float hs = size * 0.5f;
    Line(pos - glm::vec3(hs, 0, 0), pos + glm::vec3(hs, 0, 0), color, width);
    Line(pos - glm::vec3(0, hs, 0), pos + glm::vec3(0, hs, 0), color, width);
    Line(pos - glm::vec3(0, 0, hs), pos + glm::vec3(0, 0, hs), color, width);
}

void AABB(const glm::vec3& mn, const glm::vec3& mx,
          const glm::vec4& color, float width) {
    PickGroup pg;
    WireBox((mn + mx) * 0.5f, mx - mn, color, width);
}

void OBB(const glm::vec3& center, const glm::quat& orient,
         const glm::vec3& size, const glm::vec4& color, float width) {
    PickGroup pg;
    PushMatrix();
    Translate(center);
    Rotate(orient);
    WireBox({0, 0, 0}, size, color, width);
    PopMatrix();
}

void Covariance(const glm::vec3& pos, const glm::mat3& cov,
                const glm::vec4& color, float sigma, int seg) {
    glm::vec3 eigvals;
    glm::mat3 eigvecs;
    Eigen3(cov, eigvals, eigvecs);

    glm::vec3 radii = sigma * glm::vec3(
        std::sqrt(std::max(eigvals.x, 0.f)),
        std::sqrt(std::max(eigvals.y, 0.f)),
        std::sqrt(std::max(eigvals.z, 0.f)));

    PushMatrix();
    Translate(pos);
    Transform(glm::mat4(glm::vec4(eigvecs[0], 0),
                         glm::vec4(eigvecs[1], 0),
                         glm::vec4(eigvecs[2], 0),
                         glm::vec4(0, 0, 0, 1)));
    Scale(radii);
    Sphere({0, 0, 0}, 1.f, color, seg);
    PopMatrix();
}

void WireGrid(const glm::vec3& center, const glm::vec3& normal,
              float size, int divisions, const glm::vec4& color, float width) {
    PickGroup pg;
    auto n = glm::normalize(normal);
    auto u = Perpendicular(n);
    auto v = glm::cross(n, u);
    float half = size * 0.5f;
    float step = size / static_cast<float>(divisions);
    for (int i = 0; i <= divisions; ++i) {
        float t = -half + step * static_cast<float>(i);
        Line(center + u * t - v * half, center + u * t + v * half, color, width);
        Line(center - u * half + v * t, center + u * half + v * t, color, width);
    }
}

void Frustum(const glm::mat4& viewProj,
             const glm::vec4& color, float width) {
    PickGroup pg;
    glm::mat4 inv = glm::inverse(viewProj);
    auto unproject = [&](float x, float y, float z) -> glm::vec3 {
        glm::vec4 p = inv * glm::vec4(x, y, z, 1.f);
        return glm::vec3(p) / p.w;
    };
    glm::vec3 n[4] = {
        unproject(-1,-1,-1), unproject(1,-1,-1),
        unproject( 1, 1,-1), unproject(-1, 1,-1)
    };
    glm::vec3 f[4] = {
        unproject(-1,-1, 1), unproject(1,-1, 1),
        unproject( 1, 1, 1), unproject(-1, 1, 1)
    };
    for (int i = 0; i < 4; ++i) {
        Line(n[i], n[(i+1)%4], color, width);
        Line(f[i], f[(i+1)%4], color, width);
        Line(n[i], f[i], color, width);
    }
}

} // namespace Kilo::Render

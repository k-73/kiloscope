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
struct LineVert  { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; };
struct PointVert { glm::vec3 pos; glm::vec4 color; };
struct TextEntry { glm::vec3 worldPos; glm::vec4 color; std::string text; };

// ── shared GPU resources ─────────────────────────────────────────────

static Shader sMeshShader, sLineShader, sGridShader, sPointShader;
static GLuint sMeshVao = 0, sMeshVbo = 0;
static GLuint sLineVao = 0, sLineVbo = 0;
static GLuint sGridVao = 0;
static GLuint sPointVao = 0, sPointVbo = 0;

// ── per-frame render state ───────────────────────────────────────────

static glm::mat4 sView, sProj;
static glm::vec3 sCamPos, sLightDir;
static int sVpW = 1, sVpH = 1;
static std::vector<LineVert>  sLineBatch;
static std::vector<PointVert> sPointBatch;
static std::vector<TextEntry> sTextBatch;
static float sLineWidth  = 2.5f;
static float sPointSize  = 4.f;
static std::vector<glm::mat4> sMatStack = {glm::mat4(1.f)};

// ── per-scene state ──────────────────────────────────────────────────

struct SceneData { Fbo fbo; Camera cam; Environment env; GridConfig gridCfg; };

static std::string sShaderDir;
static std::unordered_map<uint32_t, std::unique_ptr<SceneData>> sScenes;
static struct { SceneData* scene{}; float cx{}, cy{}, w{}, h{}; bool hovered{}; } sFrame;
static Environment* sEnv = nullptr;

static uint32_t HashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

// ── transform helpers ────────────────────────────────────────────────

static const glm::mat4& Mat() { return sMatStack.back(); }

static glm::vec3 XformPoint(const glm::vec3& p) {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

static glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
}

static float MatScale() {
    auto& m = Mat();
    return std::max(std::max(glm::length(glm::vec3(m[0])),
                             glm::length(glm::vec3(m[1]))),
                    glm::length(glm::vec3(m[2])));
}

// ── geometry helpers ─────────────────────────────────────────────────

static glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    return glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f
                                        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
}

// ── hit testing ──────────────────────────────────────────────────────

static struct {
    enum { None, Circle, Rect } type = None;
    glm::vec2 center; float screenR;   // Circle
    glm::vec2 mn, mx;                  // Rect
} sHit;

static void SetHitCircle(const glm::vec3& worldCenter, float worldRadius) {
    auto s = WorldToScreen(worldCenter);
    if (s.x < 0.f) { sHit.type = decltype(sHit)::None; return; }
    glm::vec4 clip = sProj * sView * glm::vec4(worldCenter, 1.f);
    float r = worldRadius * sProj[1][1] * sFrame.h * 0.5f / clip.w;
    sHit = {decltype(sHit)::Circle, s, std::max(r, 4.f), {}, {}};
}

static void BeginHitRect() {
    sHit = {decltype(sHit)::Rect, {}, 0.f, glm::vec2(1e18f), glm::vec2(-1e18f)};
}

static void AddHitPoint(const glm::vec3& worldPos) {
    auto s = WorldToScreen(worldPos);
    if (s.x < 0.f) return;
    sHit.mn = glm::min(sHit.mn, s);
    sHit.mx = glm::max(sHit.mx, s);
}

static void PadHitRect(float px) {
    sHit.mn -= glm::vec2(px);
    sHit.mx += glm::vec2(px);
}

static void SetHitBox(const glm::vec3& center, const glm::vec3& halfSize) {
    BeginHitRect();
    for (int i = 0; i < 8; ++i)
        AddHitPoint(XformPoint(center + glm::vec3(
            (i & 1) ? halfSize.x : -halfSize.x,
            (i & 2) ? halfSize.y : -halfSize.y,
            (i & 4) ? halfSize.z : -halfSize.z)));
}

static void SetHitTube(const glm::vec3& a, const glm::vec3& b, float radius) {
    auto wa = XformPoint(a), wb = XformPoint(b);
    auto dir = wb - wa;
    float len = glm::length(dir);
    if (len < 1e-6f) { SetHitCircle(wa, radius * MatScale()); return; }
    auto n = dir / len;
    auto p = Perpendicular(n);
    auto s = glm::cross(n, p);
    float r = radius * MatScale();
    BeginHitRect();
    for (auto& end : {wa, wb}) {
        AddHitPoint(end + p * r); AddHitPoint(end - p * r);
        AddHitPoint(end + s * r); AddHitPoint(end - s * r);
    }
}

// ── geometry helpers ─────────────────────────────────────────────────

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

static void SetMeshUniforms(const glm::vec4& color, bool unlit = false) {
    sMeshShader.Use();
    sMeshShader.Set("uModel", glm::mat4(1.f));
    sMeshShader.Set("uView", sView);
    sMeshShader.Set("uProj", sProj);
    sMeshShader.Set("uColor", color);
    sMeshShader.Set("uLightDir", sLightDir);
    sMeshShader.Set("uCamPos", sCamPos);
    sMeshShader.Set("uUnlit", unlit ? 1 : 0);
    sMeshShader.Set("uBgColor", sEnv->bgColor);
    sMeshShader.Set("uAmbient", sEnv->ambient);
    sMeshShader.Set("uDiffuse", sEnv->diffuse);
    sMeshShader.Set("uRoughness", sEnv->roughness);
    sMeshShader.Set("uSpecular", sEnv->specular);
    sMeshShader.Set("uFresnel", sEnv->fresnel);
    sMeshShader.Set("uFogDensity", sEnv->fogDensity);
}

static void UploadMesh(const std::vector<MeshVert>& v) {
    glNamedBufferData(sMeshVbo, GLsizeiptr(v.size() * sizeof(MeshVert)),
                      v.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(sMeshVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size()));
    glBindVertexArray(0);
}

template <typename MeshT>
static void AppendMesh(std::vector<MeshVert>& out, const MeshT& mesh,
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

// ── line batching ────────────────────────────────────────────────────

static void FlushLines() {
    if (sLineBatch.empty()) return;
    sLineShader.Use();
    sLineShader.Set("uView", sView);
    sLineShader.Set("uProj", sProj);
    sLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    sLineShader.Set("uLineWidth", sLineWidth);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glNamedBufferData(sLineVbo, GLsizeiptr(sLineBatch.size() * sizeof(LineVert)),
                      sLineBatch.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(sLineVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(sLineBatch.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    sLineBatch.clear();
}

static void BatchLineGradient(const glm::vec3& a, const glm::vec3& b,
                               const glm::vec4& ca, const glm::vec4& cb, float width) {
    if (!sLineBatch.empty() && width != sLineWidth)
        FlushLines();
    sLineWidth = width;
    sLineBatch.push_back({a, b, {-1, 0}, ca});
    sLineBatch.push_back({a, b, { 1, 0}, ca});
    sLineBatch.push_back({a, b, { 1, 1}, cb});
    sLineBatch.push_back({a, b, {-1, 0}, ca});
    sLineBatch.push_back({a, b, { 1, 1}, cb});
    sLineBatch.push_back({a, b, {-1, 1}, cb});
}

static void BatchLine(const glm::vec3& a, const glm::vec3& b,
                      const glm::vec4& color, float width) {
    BatchLineGradient(a, b, color, color, width);
}

// ── point batching ───────────────────────────────────────────────────

static void FlushPoints() {
    if (sPointBatch.empty()) return;
    sPointShader.Use();
    sPointShader.Set("uView", sView);
    sPointShader.Set("uProj", sProj);
    sPointShader.Set("uPointSize", sPointSize);
    sPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glNamedBufferData(sPointVbo,
        GLsizeiptr(sPointBatch.size() * sizeof(PointVert)),
        sPointBatch.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(sPointVao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sPointBatch.size()));
    glBindVertexArray(0);

    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    sPointBatch.clear();
}

// ── projection helpers ───────────────────────────────────────────────

glm::vec2 WorldToScreen(const glm::vec3& worldPos) {
    glm::vec4 clip = sProj * sView * glm::vec4(worldPos, 1.f);
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
    if (sHit.type == decltype(sHit)::None || !sFrame.hovered) return state;

    auto& mouse = ImGui::GetIO().MousePos;

    if (sHit.type == decltype(sHit)::Circle) {
        float dx = mouse.x - sHit.center.x;
        float dy = mouse.y - sHit.center.y;
        state.hovered_ = (dx * dx + dy * dy) <= (sHit.screenR * sHit.screenR);
    } else {
        state.hovered_ = mouse.x >= sHit.mn.x && mouse.x <= sHit.mx.x
                      && mouse.y >= sHit.mn.y && mouse.y <= sHit.mx.y;
    }
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
    sShaderDir = dir;
    sMeshShader  = Shader(dir + "/Basic.vert", dir + "/Basic.frag");
    sLineShader  = Shader(dir + "/Line.vert",  dir + "/Line.frag");
    sGridShader  = Shader(dir + "/Grid.vert",  dir + "/Grid.frag");
    sPointShader = Shader(dir + "/Point.vert", dir + "/Point.frag");

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

    glCreateVertexArrays(1, &sGridVao);

    glCreateVertexArrays(1, &sPointVao);
    glCreateBuffers(1, &sPointVbo);
    SetupVao(sPointVao, sPointVbo, sizeof(PointVert), {
        {0, {3, offsetof(PointVert, pos)}},
        {1, {4, offsetof(PointVert, color)}}});
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
    scene->fbo.Resize(w, h, 8);

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

    float aspect = static_cast<float>(w) / std::max(1, h);
    sView     = cam.View();
    sProj     = cam.Projection(aspect);
    sCamPos   = cam.Position();
    sLightDir = glm::normalize(sEnv->lightDir);
    sVpW = w; sVpH = h;
    sLineBatch.clear();
    sPointBatch.clear();
    sTextBatch.clear();
    sHit.type = decltype(sHit)::None;
    sMatStack = {glm::mat4(1.f)};
}

static void DrawSun() {
    float r = sEnv->sunRadius;
    glm::vec3 sunPos = glm::normalize(sEnv->lightDir) * sEnv->sunDistance;
    glm::vec3 facing = glm::normalize(sCamPos - sunPos);

    PushMatrix();
    ResetMatrix();

    SetMeshUniforms({1.f, .98f, .85f, 1.f}, true);
    std::vector<MeshVert> sv;
    AppendMesh(sv, generator::SphereMesh(r, 16, 8),
               glm::translate(glm::mat4(1.f), sunPos));
    UploadMesh(sv);

    Circle(sunPos, facing, r * 1.8f, {1.f, .95f, .7f, .45f}, 32, 3.f);
    Circle(sunPos, facing, r * 3.f,  {1.f, .9f,  .5f, .18f}, 32, 2.f);
    Circle(sunPos, facing, r * 5.f,  {1.f, .85f, .4f, .07f}, 32, 1.5f);
    Line({0, 0, 0}, sunPos, {1.f, .95f, .7f, .12f}, 1.f);

    PopMatrix();
}

static void DrawGrid(const GridConfig& cfg, float camDist) {
    sGridShader.Use();
    sGridShader.Set("uView", sView);
    sGridShader.Set("uProj", sProj);
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
    sGridShader.Set("uFadeStart",   cfg.fadeStart);
    sGridShader.Set("uFadeEnd",     cfg.fadeEnd);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sGridVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
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
    auto wa = XformPoint(a), wb = XformPoint(b);
    BatchLine(wa, wb, color, width);
    BeginHitRect(); AddHitPoint(wa); AddHitPoint(wb); PadHitRect(width + 4.f);
}

void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width, bool closed) {
    if (count < 2) return;
    BeginHitRect();
    for (int i = 0; i < count; ++i) AddHitPoint(XformPoint(points[i]));
    PadHitRect(width + 4.f);
    for (int i = 1; i < count; ++i)
        BatchLine(XformPoint(points[i - 1]), XformPoint(points[i]), color, width);
    if (closed && count > 2)
        BatchLine(XformPoint(points[count - 1]), XformPoint(points[0]), color, width);
}

void Path(const glm::vec3* points, const glm::vec4* colors,
          int count, float width, bool closed) {
    if (count < 2) return;
    BeginHitRect();
    for (int i = 0; i < count; ++i) AddHitPoint(XformPoint(points[i]));
    PadHitRect(width + 4.f);
    for (int i = 1; i < count; ++i)
        BatchLineGradient(XformPoint(points[i - 1]), XformPoint(points[i]),
                          colors[i - 1], colors[i], width);
    if (closed && count > 2)
        BatchLineGradient(XformPoint(points[count - 1]), XformPoint(points[0]),
                          colors[count - 1], colors[0], width);
}

void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg, float width) {
    auto tc = XformPoint(center);
    auto ta = glm::normalize(XformDir(axis));
    auto ts = glm::normalize(XformDir(startDir));
    float step = glm::radians(angleDeg) / static_cast<float>(seg);
    BeginHitRect();
    for (int i = 0; i <= seg; ++i) {
        auto r0 = glm::rotate(glm::mat4(1), step * i, ta);
        auto pt = tc + glm::vec3(r0 * glm::vec4(ts * radius, 0));
        AddHitPoint(pt);
    }
    PadHitRect(width + 4.f);
    for (int i = 0; i < seg; ++i) {
        auto r0 = glm::rotate(glm::mat4(1), step * i,       ta);
        auto r1 = glm::rotate(glm::mat4(1), step * (i + 1), ta);
        BatchLine(tc + glm::vec3(r0 * glm::vec4(ts * radius, 0)),
                  tc + glm::vec3(r1 * glm::vec4(ts * radius, 0)), color, width);
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

    auto catmullRom = [](const glm::vec3& p0, const glm::vec3& p1,
                         const glm::vec3& p2, const glm::vec3& p3, float t) {
        return 0.5f * ((2.f * p1) +
            (-p0 + p2) * t +
            (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
            (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
    };

    BeginHitRect();
    for (int i = 0; i < count; ++i) AddHitPoint(XformPoint(cp[i]));
    PadHitRect(width + 4.f);

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
    if (!sPointBatch.empty() && size != sPointSize)
        FlushPoints();
    sPointSize = size;
    BeginHitRect();
    for (int i = 0; i < count; ++i) {
        auto w = XformPoint(positions[i]);
        sPointBatch.push_back({w, color});
        AddHitPoint(w);
    }
    PadHitRect(size + 2.f);
}

void Points(const glm::vec3* positions, const glm::vec4* colors,
            int count, float size) {
    if (!sPointBatch.empty() && size != sPointSize)
        FlushPoints();
    sPointSize = size;
    BeginHitRect();
    for (int i = 0; i < count; ++i) {
        auto w = XformPoint(positions[i]);
        sPointBatch.push_back({w, colors[i]});
        AddHitPoint(w);
    }
    PadHitRect(size + 2.f);
}

// ── text ─────────────────────────────────────────────────────────────

void Text(const glm::vec3& pos, const glm::vec4& color, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    auto wp = XformPoint(pos);
    sTextBatch.push_back({wp, color, buf});
    BeginHitRect(); AddHitPoint(wp); PadHitRect(40.f);
}

// ── basic geometry ───────────────────────────────────────────────────

void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color) {
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto normal = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal}});
    BeginHitRect(); AddHitPoint(ta); AddHitPoint(tb); AddHitPoint(tc);
}

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c), td = XformPoint(d);
    auto normal = glm::normalize(glm::cross(tb - ta, td - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal},
                {ta, normal}, {tc, normal}, {td, normal}});
    BeginHitRect(); AddHitPoint(ta); AddHitPoint(tb); AddHitPoint(tc); AddHitPoint(td);
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
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::SphereMesh(radius, seg, seg / 2),
               glm::translate(Mat(), center));
    UploadMesh(v);
    SetHitCircle(XformPoint(center), radius * MatScale());
}

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::BoxMesh({size.x, size.y, size.z}, {1, 1, 1}),
               glm::translate(Mat(), center));
    UploadMesh(v);
    SetHitBox(center, size * 0.5f);
}

void Cube(const glm::vec3& center, float size, const glm::vec4& color) {
    Box(center, glm::vec3(size), color);
}

void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) return;
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CappedCylinderMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(a, b));
    UploadMesh(v);
    SetHitTube(a, b, radius);
}

void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(tip - base) * 0.5f;
    if (halfLen < 1e-6f) return;
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CappedConeMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(base, tip));
    UploadMesh(v);
    SetHitTube(base, tip, radius);
}

void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; }
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::CapsuleMesh(radius, halfLen, seg, 1, seg / 2),
               Mat() * ZAlign(a, b));
    UploadMesh(v);
    SetHitTube(a, b, radius);
}

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(v);
    auto n = glm::normalize(axis);
    auto p = Perpendicular(n);
    auto s = glm::cross(n, p);
    float r = majorR + minorR;
    BeginHitRect();
    AddHitPoint(XformPoint(center + p * r)); AddHitPoint(XformPoint(center - p * r));
    AddHitPoint(XformPoint(center + s * r)); AddHitPoint(XformPoint(center - s * r));
    AddHitPoint(XformPoint(center + n * minorR)); AddHitPoint(XformPoint(center - n * minorR));
}

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(radius, 0.0, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(v);
    auto n = glm::normalize(normal);
    auto p = Perpendicular(n);
    auto s = glm::cross(n, p);
    BeginHitRect();
    AddHitPoint(XformPoint(center + p * radius)); AddHitPoint(XformPoint(center - p * radius));
    AddHitPoint(XformPoint(center + s * radius)); AddHitPoint(XformPoint(center - s * radius));
}

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(v);
    auto n = glm::normalize(normal);
    auto p = Perpendicular(n);
    auto s = glm::cross(n, p);
    BeginHitRect();
    AddHitPoint(XformPoint(center + p * outerR)); AddHitPoint(XformPoint(center - p * outerR));
    AddHitPoint(XformPoint(center + s * outerR)); AddHitPoint(XformPoint(center - s * outerR));
}

// ── custom mesh ──────────────────────────────────────────────────────

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          const uint32_t* indices, int indexCount, const glm::vec4& color) {
    SetMeshUniforms(color);
    auto& m = Mat();
    auto nmat = glm::transpose(glm::inverse(glm::mat3(m)));
    std::vector<MeshVert> v;
    v.reserve(indexCount);
    BeginHitRect();
    for (int i = 0; i < indexCount; ++i) {
        uint32_t idx = indices[i];
        auto wp = glm::vec3(m * glm::vec4(verts[idx], 1.f));
        v.push_back({wp, glm::normalize(nmat * normals[idx])});
        AddHitPoint(wp);
    }
    UploadMesh(v);
}

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          int vertCount, const glm::vec4& color) {
    SetMeshUniforms(color);
    auto& m = Mat();
    auto nmat = glm::transpose(glm::inverse(glm::mat3(m)));
    std::vector<MeshVert> v;
    v.reserve(vertCount);
    BeginHitRect();
    for (int i = 0; i < vertCount; ++i) {
        auto wp = glm::vec3(m * glm::vec4(verts[i], 1.f));
        v.push_back({wp, glm::normalize(nmat * normals[i])});
        AddHitPoint(wp);
    }
    UploadMesh(v);
}

// ── wireframe ────────────────────────────────────────────────────────

void WireBox(const glm::vec3& center, const glm::vec3& size,
             const glm::vec4& color, float width) {
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
    SetHitBox(center, hs);
}

void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg, float width) {
    Circle(center, {1, 0, 0}, radius, color, seg, width);
    Circle(center, {0, 1, 0}, radius, color, seg, width);
    Circle(center, {0, 0, 1}, radius, color, seg, width);
    SetHitCircle(XformPoint(center), radius * MatScale());
}

void WireCylinder(const glm::vec3& a, const glm::vec3& b,
                  float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
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
    SetHitTube(a, b, radius);
}

void WireCone(const glm::vec3& base, const glm::vec3& tip,
              float radius, const glm::vec4& color, int seg, float width) {
    auto axis = tip - base;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    auto dir  = axis / len;
    auto perp = Perpendicular(dir);
    auto side = glm::cross(dir, perp);

    Circle(base, dir, radius, color, seg, width);

    for (int i = 0; i < 4; ++i) {
        float angle = glm::half_pi<float>() * static_cast<float>(i);
        auto d = perp * std::cos(angle) + side * std::sin(angle);
        Line(base + d * radius, tip, color, width);
    }
    SetHitTube(base, tip, radius);
}

void WireCapsule(const glm::vec3& a, const glm::vec3& b,
                 float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) { WireSphere((a + b) * 0.5f, radius, color, seg, width); return; }
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
    // extend tube by cap radius
    auto n = dir * radius;
    SetHitTube(a - n, b + n, radius);
}

// ── composite ────────────────────────────────────────────────────────

void Arrow(const glm::vec3& from, const glm::vec3& to,
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
    SetHitTube(from, to, headR);
}

void Axes(const glm::vec3& origin, float len) {
    float s = len * 0.025f, h = len * 0.07f;
    Arrow(origin, origin + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
    BeginHitRect();
    AddHitPoint(XformPoint(origin));
    AddHitPoint(XformPoint(origin + glm::vec3(len, 0, 0)));
    AddHitPoint(XformPoint(origin + glm::vec3(0, len, 0)));
    AddHitPoint(XformPoint(origin + glm::vec3(0, 0, len)));
    PadHitRect(h * MatScale() * sProj[1][1] * sFrame.h * 0.5f /
               (sProj * sView * glm::vec4(XformPoint(origin), 1.f)).w);
}

void Frame(const glm::mat4& pose, float len) {
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
    float hs = size * 0.5f;
    Line(pos - glm::vec3(hs, 0, 0), pos + glm::vec3(hs, 0, 0), color, width);
    Line(pos - glm::vec3(0, hs, 0), pos + glm::vec3(0, hs, 0), color, width);
    Line(pos - glm::vec3(0, 0, hs), pos + glm::vec3(0, 0, hs), color, width);
    BeginHitRect();
    AddHitPoint(XformPoint(pos + glm::vec3( hs, 0, 0)));
    AddHitPoint(XformPoint(pos + glm::vec3(-hs, 0, 0)));
    AddHitPoint(XformPoint(pos + glm::vec3(0,  hs, 0)));
    AddHitPoint(XformPoint(pos + glm::vec3(0, -hs, 0)));
    AddHitPoint(XformPoint(pos + glm::vec3(0, 0,  hs)));
    AddHitPoint(XformPoint(pos + glm::vec3(0, 0, -hs)));
    PadHitRect(width + 4.f);
}

void AABB(const glm::vec3& mn, const glm::vec3& mx,
          const glm::vec4& color, float width) {
    WireBox((mn + mx) * 0.5f, mx - mn, color, width);
}

void OBB(const glm::vec3& center, const glm::quat& orient,
         const glm::vec3& size, const glm::vec4& color, float width) {
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
    BeginHitRect();
    AddHitPoint(XformPoint(center + (u + v) * half));
    AddHitPoint(XformPoint(center + (u - v) * half));
    AddHitPoint(XformPoint(center + (-u + v) * half));
    AddHitPoint(XformPoint(center + (-u - v) * half));
    PadHitRect(width + 2.f);
}

void Frustum(const glm::mat4& viewProj,
             const glm::vec4& color, float width) {
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
    BeginHitRect();
    for (int i = 0; i < 4; ++i) { AddHitPoint(n[i]); AddHitPoint(f[i]); }
    PadHitRect(width + 2.f);
}

} // namespace Kilo::Render

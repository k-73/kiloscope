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
#include <memory>
#include <unordered_map>
#include <vector>

namespace Kilo::Render {

// ── types ────────────────────────────────────────────────────────────

struct MeshVert { glm::vec3 pos, normal; };
struct LineVert { glm::vec3 pos, otherEnd; glm::vec2 expand; glm::vec4 color; };

// ── shared GPU resources ─────────────────────────────────────────────

static Shader sMeshShader, sLineShader, sGridShader;
static GLuint sMeshVao = 0, sMeshVbo = 0;
static GLuint sLineVao = 0, sLineVbo = 0;
static GLuint sGridVao = 0;

// ── per-frame render state ───────────────────────────────────────────

static glm::mat4 sView, sProj;
static glm::vec3 sCamPos, sLightDir;
static int sVpW = 1, sVpH = 1;
static std::vector<LineVert> sLineBatch;
static float sLineWidth = 2.5f;
static std::vector<glm::mat4> sMatStack = {glm::mat4(1.f)};

// ── per-scene state ──────────────────────────────────────────────────

struct SceneData { Fbo fbo; Camera cam; Environment env; GridConfig gridCfg; };

static std::string sShaderDir;
static std::unordered_map<std::string, std::unique_ptr<SceneData>> sScenes;
static struct { SceneData* scene{}; float cx{}, cy{}, w{}, h{}; } sFrame;
static Environment* sEnv = nullptr;

// ── transform helpers ────────────────────────────────────────────────

static const glm::mat4& Mat() { return sMatStack.back(); }

static glm::vec3 XformPoint(const glm::vec3& p) {
    return glm::vec3(Mat() * glm::vec4(p, 1.f));
}

static glm::vec3 XformDir(const glm::vec3& d) {
    return glm::mat3(Mat()) * d;
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

static glm::vec3 Perpendicular(const glm::vec3& v) {
    auto n = glm::normalize(v);
    return glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f
                                        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
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

static void BatchLine(const glm::vec3& a, const glm::vec3& b,
                      const glm::vec4& color, float width) {
    if (!sLineBatch.empty() && width != sLineWidth)
        FlushLines();
    sLineWidth = width;
    sLineBatch.push_back({a, b, {-1, 0}, color});
    sLineBatch.push_back({a, b, { 1, 0}, color});
    sLineBatch.push_back({a, b, { 1, 1}, color});
    sLineBatch.push_back({a, b, {-1, 0}, color});
    sLineBatch.push_back({a, b, { 1, 1}, color});
    sLineBatch.push_back({a, b, {-1, 1}, color});
}

// ── scene viewport ───────────────────────────────────────────────────

void Init(const std::string& dir) {
    sShaderDir = dir;
    sMeshShader = Shader(dir + "/Basic.vert", dir + "/Basic.frag");
    sLineShader = Shader(dir + "/Line.vert",  dir + "/Line.frag");
    sGridShader = Shader(dir + "/Grid.vert",  dir + "/Grid.frag");

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
}

static SceneData& GetScene(const char* name) {
    auto& s = sScenes[name];
    if (!s) s = std::make_unique<SceneData>();
    return *s;
}

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
    auto& scene = sScenes[name];
    if (!scene) scene = std::make_unique<SceneData>();

    // viewport size
    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(cfg.width  > 0 ? cfg.width  : avail.x));
    int h = std::max(1, static_cast<int>(cfg.height > 0 ? cfg.height : avail.y));
    scene->fbo.Resize(w, h, 8);

    // input region
    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    auto& io  = ImGui::GetIO();
    auto& cam = scene->cam;

    if (ImGui::IsItemHovered() && io.MouseWheel != 0)
        cam.Zoom(io.MouseWheel);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    // begin render pass
    sFrame = { scene.get(), cursor.x, cursor.y, size.x, size.y };
    sEnv = &scene->env;

    const auto& bg = sEnv->bgColor;
    scene->fbo.Bind(bg.r, bg.g, bg.b);

    float aspect = static_cast<float>(w) / std::max(1, h);
    sView     = cam.View();
    sProj     = cam.Projection(aspect);
    sCamPos   = cam.Position();
    sLightDir = glm::normalize(sEnv->lightDir);
    sVpW = w;
    sVpH = h;
    sLineBatch.clear();
    sMatStack = {glm::mat4(1.f)};
}

static void DrawSun() {
    float r = sEnv->sunRadius;
    glm::vec3 sunPos = glm::normalize(sEnv->lightDir) * sEnv->sunDistance;
    glm::vec3 facing = glm::normalize(sCamPos - sunPos);

    PushMatrix();
    ResetMatrix();

    // unlit sphere core
    SetMeshUniforms({1.f, .98f, .85f, 1.f}, true);
    std::vector<MeshVert> sv;
    AppendMesh(sv, generator::SphereMesh(r, 16, 8),
               glm::translate(glm::mat4(1.f), sunPos));
    UploadMesh(sv);

    // glow rings (billboard — facing camera)
    Circle(sunPos, facing, r * 1.8f, {1.f, .95f, .7f, .45f}, 32, 3.f);
    Circle(sunPos, facing, r * 3.f,  {1.f, .9f,  .5f, .18f}, 32, 2.f);
    Circle(sunPos, facing, r * 5.f,  {1.f, .85f, .4f, .07f}, 32, 1.5f);

    // direction indicator
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
    FlushLines();
    auto& g = sFrame.scene->gridCfg;
    if (g.enabled)
        DrawGrid(g, sFrame.scene->cam.Distance());
    sFrame.scene->fbo.Resolve();
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(sFrame.scene->fbo.Texture())),
                 {sFrame.w, sFrame.h}, {0, 1}, {1, 0});
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
    BatchLine(XformPoint(a), XformPoint(b), color, width);
}

void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width, bool closed) {
    if (count < 2) return;
    for (int i = 1; i < count; ++i)
        Line(points[i - 1], points[i], color, width);
    if (closed && count > 2)
        Line(points[count - 1], points[0], color, width);
}

void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg, float width) {
    auto tc = XformPoint(center);
    auto ta = glm::normalize(XformDir(axis));
    auto ts = glm::normalize(XformDir(startDir));
    float step = glm::radians(angleDeg) / static_cast<float>(seg);
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

// ── basic geometry ───────────────────────────────────────────────────

void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color) {
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto normal = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal}});
}

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
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
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::SphereMesh(radius, seg, seg / 2),
               glm::translate(Mat(), center));
    UploadMesh(v);
}

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::BoxMesh({size.x, size.y, size.z}, {1, 1, 1}),
               glm::translate(Mat(), center));
    UploadMesh(v);
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
}

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(v);
}

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(radius, 0.0, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(v);
}

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg) {
    SetMeshUniforms(color);
    std::vector<MeshVert> v;
    AppendMesh(v, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
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
    // bottom, top, verticals
    Line(c[0],c[1],color,width); Line(c[1],c[2],color,width);
    Line(c[2],c[3],color,width); Line(c[3],c[0],color,width);
    Line(c[4],c[5],color,width); Line(c[5],c[6],color,width);
    Line(c[6],c[7],color,width); Line(c[7],c[4],color,width);
    Line(c[0],c[4],color,width); Line(c[1],c[5],color,width);
    Line(c[2],c[6],color,width); Line(c[3],c[7],color,width);
}

void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg, float width) {
    Circle(center, {1, 0, 0}, radius, color, seg, width);
    Circle(center, {0, 1, 0}, radius, color, seg, width);
    Circle(center, {0, 0, 1}, radius, color, seg, width);
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
}

void Axes(const glm::vec3& origin, float len) {
    float s = len * 0.025f, h = len * 0.07f;
    Arrow(origin, origin + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
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
}

void AABB(const glm::vec3& mn, const glm::vec3& mx,
          const glm::vec4& color, float width) {
    WireBox((mn + mx) * 0.5f, mx - mn, color, width);
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
    // near + far faces
    for (int i = 0; i < 4; ++i) {
        Line(n[i], n[(i+1)%4], color, width);
        Line(f[i], f[(i+1)%4], color, width);
        Line(n[i], f[i], color, width);
    }
}

} // namespace Kilo::Render

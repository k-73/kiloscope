#include "Render/DrawState.hpp"
#include <GLFW/glfw3.h>
#include <generator/SphereMesh.hpp>
#include <generator/BoxMesh.hpp>
#include <generator/CappedCylinderMesh.hpp>
#include <generator/CappedConeMesh.hpp>
#include <generator/CapsuleMesh.hpp>
#include <generator/TorusMesh.hpp>
#include <generator/DiskMesh.hpp>

namespace Kilo::Render {

// ── FBO implementations ──────────────────────────────────────────────

void PickFbo::Resize(int nw, int nh) {
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
    // Double-buffered PBOs for async pick readback
    glCreateBuffers(2, pbo);
    for (auto p : pbo)
        glNamedBufferStorage(p, sizeof(uint32_t), nullptr, GL_MAP_READ_BIT);
    pboIdx = 0;
    pboReady = false;
}
void PickFbo::Bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

void PickFbo::Clear() {
    Bind();
    GLuint zero = 0;  glClearBufferuiv(GL_COLOR, 0, &zero);
    float  one  = 1.f; glClearBufferfv(GL_DEPTH, 0, &one);
}

void PickFbo::BeginAsyncRead(int screenX, int screenY) {
    int fy = h - 1 - screenY;
    if (screenX < 0 || screenX >= w || fy < 0 || fy >= h) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIdx]);
    glReadPixels(screenX, fy, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

uint32_t PickFbo::FinishAsyncRead() {
    if (!pboReady) return 0;
    int readIdx = 1 - pboIdx; // read from the OTHER PBO (previous frame)
    auto* ptr = static_cast<uint32_t*>(glMapNamedBufferRange(pbo[readIdx], 0, sizeof(uint32_t), GL_MAP_READ_BIT));
    uint32_t id = ptr ? *ptr : 0;
    glUnmapNamedBuffer(pbo[readIdx]);
    return id;
}
void PickFbo::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (color) { glDeleteTextures(1, &color); color = 0; }
    if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
    if (pbo[0]) { glDeleteBuffers(2, pbo); pbo[0] = pbo[1] = 0; }
    w = h = 0;
    pboReady = false;
}

// ── mesh cache (unit-size meshes, generated on first use) ────────────

static std::unordered_map<int, IndexedMesh> sSphereCache;
static std::unordered_map<int, IndexedMesh> sCylinderCache;
static std::unordered_map<int, IndexedMesh> sConeCache;
static std::unordered_map<int, IndexedMesh> sCapsuleCache;
static std::unordered_map<int, IndexedMesh> sTorusCache;
static std::unordered_map<int, IndexedMesh> sDiskCache;
static IndexedMesh sBoxCache;

template <typename GenT>
static void BuildCache(IndexedMesh& out, const GenT& gen) {
    for (auto it = gen.vertices(); !it.done(); it.next()) {
        auto v = it.generate();
        out.pos.push_back(glm::vec3(v.position));
        out.nrm.push_back(glm::vec3(v.normal));
        out.uv.push_back(glm::vec2(v.texCoord));
    }
    for (auto it = gen.triangles(); !it.done(); it.next()) {
        auto t = it.generate();
        out.tri.push_back({t.vertices[0], t.vertices[1], t.vertices[2]});
    }
}

const IndexedMesh& GetUnitSphere(int seg) {
    auto& m = sSphereCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::SphereMesh(1.f, seg, seg / 2));
    return m;
}

const IndexedMesh& GetUnitBox() {
    if (sBoxCache.pos.empty()) BuildCache(sBoxCache, generator::BoxMesh({.5f, .5f, .5f}, {1, 1, 1}));
    return sBoxCache;
}

const IndexedMesh& GetUnitCylinder(int seg) {
    auto& m = sCylinderCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CappedCylinderMesh(1.f, 1.f, seg, 1, 1));
    return m;
}

const IndexedMesh& GetUnitCone(int seg) {
    auto& m = sConeCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CappedConeMesh(1.f, 1.f, seg, 1, 1));
    return m;
}

const IndexedMesh& GetUnitCapsule(int seg) {
    auto& m = sCapsuleCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CapsuleMesh(1.f, 1.f, seg, 1, seg / 2));
    return m;
}

const IndexedMesh& GetUnitTorus(int seg) {
    auto& m = sTorusCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::TorusMesh(1.f, 1.f, seg / 2, seg));
    return m;
}

const IndexedMesh& GetUnitDisk(int seg) {
    auto& m = sDiskCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::DiskMesh(1.f, 0.0, seg, 1));
    return m;
}

// ── cached uniform locations (filled once in Init) ───────────────────

struct LightLocs { GLint pos, color, range; };
static LightLocs sLightLocs[kMaxPointLights];
static GLint sNumLightsLoc = -1;

// ── GPU setup ────────────────────────────────────────────────────────

static void SetupVao(GLuint vao, GLuint vbo, GLsizei stride,
                     std::initializer_list<std::pair<GLuint, std::pair<GLint, GLuint>>> attrs) {
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
    for (auto& [idx, spec] : attrs) {
        glEnableVertexArrayAttrib(vao, idx);
        glVertexArrayAttribFormat(vao, idx, spec.first, GL_FLOAT, GL_FALSE, spec.second);
        glVertexArrayAttribBinding(vao, idx, 0);
    }
}

void SetMeshFrameUniforms() {
    if (ctx().meshFrameReady) return;
    auto& env = ctx().env;
    sMeshShader.Use();
    sMeshShader.Set("uViewProj", sViewProj);
    sMeshShader.Set("uLightDir", sLightDir);
    sMeshShader.Set("uCamPos", sCamPos);
    sMeshShader.Set("uBgColor", env.bgColor);
    sMeshShader.Set("uAmbient", env.ambient);
    sMeshShader.Set("uDiffuse", env.diffuse);
    sMeshShader.Set("uRoughness", env.roughness);
    sMeshShader.Set("uSpecular", env.specular);
    sMeshShader.Set("uFresnel", env.fresnel);
    sMeshShader.Set("uFogDensity", env.fogDensity);
    glUniform1i(sNumLightsLoc, ctx().numPointLights);
    for (int i = 0; i < ctx().numPointLights; ++i) {
        auto& light = ctx().pointLights[i];
        glUniform3fv(sLightLocs[i].pos,   1, &light.pos.x);
        glUniform3fv(sLightLocs[i].color, 1, &light.color.x);
        glUniform1f (sLightLocs[i].range, light.range);
    }
    ctx().meshFrameReady = true;
}

void SetMeshUniforms(const glm::vec4& color, bool unlit) {
    ctx().currentColor = color;
    // Encode shading mode: 0=lit, 1=unlit, 2=emissive, 3=glow
    ctx().currentShadingMode = ctx().glow ? 3 : (ctx().emissive ? 2 : (unlit ? 1 : 0));
}

// ── pick pass helpers ────────────────────────────────────────────────

static void BeginPickPass() {
    ctx().pickFbo.Bind();
    glDepthMask(GL_TRUE);
}

static void EndPickPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, ctx().fbo.Handle());
    glViewport(0, 0, sVpW, sVpH);
}

void UploadMesh(const std::vector<MeshVert>& v) {
    auto& s = ctx();
    auto count  = static_cast<GLsizei>(v.size());
    auto offset = static_cast<GLsizei>(s.vboAccum.size());
    s.vboAccum.insert(s.vboAccum.end(), v.begin(), v.end());
    s.drawList.push_back({offset, count, s.currentColor, s.currentShadingMode, s.activePickId});
    s.stats.vertices += count;

    // Auto-generate glow sphere for emissive meshes
    bool emissive = s.emissive;
    float glowR   = s.glowRadius;
    s.emissive = false;
    s.glow     = false;
    s.glowRadius = 0.f;

    if (emissive && count > 0) {
        // Compute centroid + bounding radius in one pass
        glm::vec3 centroid(0.f);
        for (GLsizei i = offset; i < offset + count; ++i)
            centroid += s.vboAccum[i].pos;
        centroid /= static_cast<float>(count);

        if (glowR <= 0.f) {
            float maxR = 0.f;
            for (GLsizei i = offset; i < offset + count; ++i)
                maxR = glm::max(maxR, glm::length(s.vboAccum[i].pos - centroid));
            glowR = glm::max(maxR * kGlowRadiusScale, kGlowRadiusMin);
        }

        sMeshScratch.clear();
        AppendFromCache(sMeshScratch, GetUnitSphere(16),
                        glm::scale(glm::translate(glm::mat4(1.f), centroid), glm::vec3(glowR)));
        auto glowOffset = static_cast<GLsizei>(s.vboAccum.size());
        auto glowCount  = static_cast<GLsizei>(sMeshScratch.size());
        s.vboAccum.insert(s.vboAccum.end(), sMeshScratch.begin(), sMeshScratch.end());
        s.drawList.push_back({glowOffset, glowCount,
            {s.currentColor.r, s.currentColor.g, s.currentColor.b, kGlowAlpha}, 3, 0});
        s.stats.vertices += glowCount;
    }
}

// ── line batching ────────────────────────────────────────────────────

void FlushLines() {
    if (ctx().lineBatch.empty()) return;
    auto count = static_cast<GLsizei>(ctx().lineBatch.size());
    UploadVbo(sLineVbo, sLineVboCap, ctx().lineBatch.data(), GLsizeiptr(count * sizeof(LineVert)));
    ++ctx().stats.drawCalls;
    ctx().stats.lineSegments += count / 6; // 6 verts = 2 triangles per line segment

    sLineShader.Use();
    sLineShader.Set("uView", sView);
    sLineShader.Set("uProj", sProj);
    sLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    sLineShader.Set("uLineWidth", ctx().lineWidth);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sLineVao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    if (ctx().pickEnabled) {
        BeginPickPass();
        sPickLineShader.Use();
        sPickLineShader.Set("uView", sView);
        sPickLineShader.Set("uProj", sProj);
        sPickLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        sPickLineShader.Set("uLineWidth", ctx().lineWidth);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(sLineVao);
        glDrawArrays(GL_TRIANGLES, 0, count);
        glEnable(GL_CULL_FACE);
        ++ctx().stats.pickDrawCalls;
        EndPickPass();
    }
    ctx().lineBatch.clear();
}

void BatchLineGradient(const glm::vec3& a, const glm::vec3& b,
                        const glm::vec4& ca, const glm::vec4& cb, float width) {
    if (!ctx().lineBatch.empty() && width != ctx().lineWidth) FlushLines();
    ctx().lineWidth = width;
    uint32_t pid = ctx().activePickId;
    auto& batch = ctx().lineBatch;
    batch.push_back({a, b, {-1, 0}, ca, pid});
    batch.push_back({a, b, { 1, 0}, ca, pid});
    batch.push_back({a, b, { 1, 1}, cb, pid});
    batch.push_back({a, b, {-1, 0}, ca, pid});
    batch.push_back({a, b, { 1, 1}, cb, pid});
    batch.push_back({a, b, {-1, 1}, cb, pid});
}

void BatchLine(const glm::vec3& a, const glm::vec3& b,
               const glm::vec4& color, float width) {
    BatchLineGradient(a, b, color, color, width);
}

// ── point batching ───────────────────────────────────────────────────

void FlushPoints() {
    if (ctx().pointBatch.empty()) return;
    auto count = static_cast<GLsizei>(ctx().pointBatch.size());
    UploadVbo(sPointVbo, sPointVboCap, ctx().pointBatch.data(), GLsizeiptr(count * sizeof(PointVert)));
    ++ctx().stats.drawCalls;
    ctx().stats.points += count;

    sPointShader.Use();
    sPointShader.Set("uView", sView);
    sPointShader.Set("uProj", sProj);
    sPointShader.Set("uPointSize", ctx().pointSize);
    sPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sPointVao);
    glDrawArrays(GL_POINTS, 0, count);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    if (ctx().pickEnabled) {
        BeginPickPass();
        sPickPointShader.Use();
        sPickPointShader.Set("uView", sView);
        sPickPointShader.Set("uProj", sProj);
        sPickPointShader.Set("uPointSize", ctx().pointSize);
        sPickPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(sPointVao);
        glDrawArrays(GL_POINTS, 0, count);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_CULL_FACE);
        ++ctx().stats.pickDrawCalls;
        EndPickPass();
    }
    ctx().pointBatch.clear();
}

// ── projection + interaction ─────────────────────────────────────────

glm::vec2 WorldToScreen(const glm::vec3& worldPos) {
    glm::vec4 clip = sViewProj * glm::vec4(worldPos, 1.f);
    if (clip.w <= 0.f) return {-1.f, -1.f};
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return { sFrame.cx + (ndc.x * 0.5f + 0.5f) * sFrame.w,
             sFrame.cy + (1.f - (ndc.y * 0.5f + 0.5f)) * sFrame.h };
}

static void ConsumePick() {
    if (!sFrame.scene) return;
    sFrame.scene->hoveredPickId = 0;
    sFrame.scene->pickConsumed  = true;
}

bool EventState::Clicked(Button btn) const {
    if (!hovered_ || !ImGui::IsMouseClicked(btn)) return false;
    ConsumePick();
    return true;
}

bool EventState::DoubleClicked(Button btn) const {
    if (!hovered_ || !ImGui::IsMouseDoubleClicked(btn)) return false;
    ConsumePick();
    return true;
}

bool EventState::Dragging(Button btn) const {
    return sFrame.scene && pickId_ != 0
        && pickId_ == sFrame.scene->dragPickId[btn]
        && ImGui::IsMouseDragging(btn);
}

bool EventState::Released(Button btn) const {
    return sFrame.scene && pickId_ != 0
        && pickId_ == sFrame.scene->dragPickId[btn]
        && ImGui::IsMouseReleased(btn);
}

glm::vec2 EventState::DragDelta(Button btn) const {
    if (!Dragging(btn)) return {0.f, 0.f};
    auto d = ImGui::GetMouseDragDelta(btn);
    return {d.x, d.y};
}

EventState Event() {
    EventState state;
    if (!sFrame.scene) return state;
    state.pickId_  = ctx().activePickId;
    state.hovered_ = sFrame.hovered
                  && state.pickId_ != 0
                  && state.pickId_ == ctx().hoveredPickId;
    return state;
}

// ── text overlay ─────────────────────────────────────────────────────

static void FlushText() {
    if (ctx().textBatch.empty()) return;
    auto* dl = ImGui::GetWindowDrawList();
    ImGui::PushClipRect({sFrame.cx, sFrame.cy}, {sFrame.cx + sFrame.w, sFrame.cy + sFrame.h}, true);
    for (auto& e : ctx().textBatch) {
        auto screen = WorldToScreen(e.worldPos);
        if (screen.x < 0.f) continue;
        dl->AddText({screen.x, screen.y},
            ImGui::ColorConvertFloat4ToU32({e.color.r, e.color.g, e.color.b, e.color.a}),
            e.text.c_str());
    }
    ImGui::PopClipRect();
    ctx().textBatch.clear();
}

// ── Init ─────────────────────────────────────────────────────────────

void Init(const std::string& dir) {
    sMeshShader      = Shader(dir + "/Basic.vert",     dir + "/Basic.frag");
    sLineShader      = Shader(dir + "/Line.vert",      dir + "/Line.frag");
    sGridShader      = Shader(dir + "/Grid.vert",      dir + "/Grid.frag");
    sPointShader     = Shader(dir + "/Point.vert",     dir + "/Point.frag");
    sPickMeshShader  = Shader(dir + "/Pick.vert",      dir + "/Pick.frag");
    sPickLineShader  = Shader(dir + "/PickLine.vert",  dir + "/PickLine.frag");
    sPickPointShader = Shader(dir + "/PickPoint.vert", dir + "/PickPoint.frag");

    // Cache point light uniform locations
    auto prog = sMeshShader.Id();
    sNumLightsLoc = glGetUniformLocation(prog, "uNumPointLights");
    char buf[48];
    for (int i = 0; i < kMaxPointLights; ++i) {
        std::snprintf(buf, sizeof(buf), "uPLPos[%d]", i);
        sLightLocs[i].pos = glGetUniformLocation(prog, buf);
        std::snprintf(buf, sizeof(buf), "uPLColor[%d]", i);
        sLightLocs[i].color = glGetUniformLocation(prog, buf);
        std::snprintf(buf, sizeof(buf), "uPLRange[%d]", i);
        sLightLocs[i].range = glGetUniformLocation(prog, buf);
    }

    glCreateVertexArrays(1, &sMeshVao);
    glCreateBuffers(1, &sMeshVbo);
    SetupVao(sMeshVao, sMeshVbo, sizeof(MeshVert), {
        {0, {3, offsetof(MeshVert, pos)}}, {1, {3, offsetof(MeshVert, normal)}},
        {2, {2, offsetof(MeshVert, uv)}}});

    glCreateVertexArrays(1, &sLineVao);
    glCreateBuffers(1, &sLineVbo);
    SetupVao(sLineVao, sLineVbo, sizeof(LineVert), {
        {0, {3, offsetof(LineVert, pos)}}, {1, {3, offsetof(LineVert, otherEnd)}},
        {2, {2, offsetof(LineVert, expand)}}, {3, {4, offsetof(LineVert, color)}}});
    // Integer attributes (pickId) need IFormat — can't use SetupVao
    auto bindPickAttr = [](GLuint vao, GLuint idx, GLuint offset) {
        glEnableVertexArrayAttrib(vao, idx);
        glVertexArrayAttribIFormat(vao, idx, 1, GL_UNSIGNED_INT, offset);
        glVertexArrayAttribBinding(vao, idx, 0);
    };
    bindPickAttr(sLineVao, 4, offsetof(LineVert, pickId));

    glCreateVertexArrays(1, &sGridVao);

    glCreateVertexArrays(1, &sPointVao);
    glCreateBuffers(1, &sPointVbo);
    SetupVao(sPointVao, sPointVbo, sizeof(PointVert), {
        {0, {3, offsetof(PointVert, pos)}}, {1, {4, offsetof(PointVert, color)}}});
    bindPickAttr(sPointVao, 2, offsetof(PointVert, pickId));
}

// ── Shutdown ─────────────────────────────────────────────────────────

void Shutdown() {
    sScenes.clear();
    sMeshShader = {}; sLineShader = {}; sGridShader = {}; sPointShader = {};
    sPickMeshShader = {}; sPickLineShader = {}; sPickPointShader = {};
    GLuint vaos[] = {sMeshVao, sLineVao, sGridVao, sPointVao};
    GLuint vbos[] = {sMeshVbo, sLineVbo, sPointVbo};
    glDeleteVertexArrays(4, vaos);
    glDeleteBuffers(3, vbos);
    sFrame = {};
}

// ── scene getters ────────────────────────────────────────────────────

static SceneData& GetScene(uint32_t id) { auto& s = sScenes[id]; if (!s) s = std::make_unique<SceneData>(); return *s; }
static SceneData& GetScene(const char* name) { return GetScene(HashName(name)); }

Camera& GetCamera() { return ctx().cam; }
Camera& GetCamera(const char* name) { return GetScene(name).cam; }
Environment& GetEnvironment() { return ctx().env; }
Environment& GetEnvironment(const char* name) { return GetScene(name).env; }

int PointLight(const glm::vec3& pos, const glm::vec3& color, float range) {
    if (ctx().numPointLights >= kMaxPointLights) return -1;
    int idx = ctx().numPointLights++;
    ctx().pointLights[idx] = {pos, color, range};
    return idx;
}

int  GetPointLightCount() { return sFrame.scene ? ctx().numPointLights : 0; }
PointLightInfo* GetPointLights() { return sFrame.scene ? ctx().pointLights : nullptr; }

void SetNextEmissive(float glowRadius) {
    ctx().emissive = true;
    ctx().glowRadius = glowRadius;
}
void SetNextGlow() { ctx().glow = true; }

const Stats& GetStats() {
    static const Stats empty{};
    return sFrame.scene ? ctx().stats : empty;
}

void Grid() { ctx().gridCfg.enabled = true; }
void Grid(const GridConfig& cfg) { ctx().gridCfg = cfg; }
GridConfig& GetGrid() { return ctx().gridCfg; }
GridConfig& GetGrid(const char* name) { return GetScene(name).gridCfg; }

// ── Begin ────────────────────────────────────────────────────────────

void Begin(const char* name, const ViewportConfig& cfg) {
    auto& scene = sScenes[HashName(name)];
    if (!scene) scene = std::make_unique<SceneData>();

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(cfg.width > 0 ? cfg.width : avail.x));
    int h = std::max(1, static_cast<int>(cfg.height > 0 ? cfg.height : avail.y));
    scene->fbo.Resize(w, h, 16);
    scene->pickFbo.Resize(w, h);
    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

    auto& io = ImGui::GetIO();
    auto& cam = scene->cam;
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    bool fly = active && ImGui::IsMouseDown(ImGuiMouseButton_Right);

    if (hovered && io.MouseWheel != 0.f) cam.Zoom(io.MouseWheel);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        if (shift) cam.Pan(io.MouseDelta.x, io.MouseDelta.y);
        else       cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    }

    { // Fly mode cursor lock
        static bool sFlyLocked = false;
        auto* win = glfwGetCurrentContext();
        if (fly && !sFlyLocked) { glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED); sFlyLocked = true; }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && sFlyLocked) { glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL); sFlyLocked = false; }
    }
    if (fly) {
        cam.FlyLook(io.MouseDelta.x, io.MouseDelta.y);
        float f = 0, r = 0, u = 0;
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))    f += 1;
        if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))  f -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) r += 1;
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))  r -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space))      u += 1;
        if (ImGui::IsKeyDown(ImGuiKey_Q))                                          u -= 1;
        if (f || r || u) cam.FlyMove(f, r, u, io.DeltaTime);
    }

    sFrame = {scene.get(), cursor.x, cursor.y, size.x, size.y, hovered, fly};

    // Drag tracking: start on click over hovered object
    for (int b = 0; b < kButtonCount; ++b) {
        if (hovered && ImGui::IsMouseClicked(b) && scene->hoveredPickId != 0)
            scene->dragPickId[b] = scene->hoveredPickId;
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    auto& bg = scene->env.bgColor;
    scene->fbo.Bind(bg.r, bg.g, bg.b);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    float aspect = static_cast<float>(w) / std::max(1, h);
    sView = cam.View();
    sProj  = cam.Projection(aspect);
    sViewProj = sProj * sView;
    sCamPos   = cam.Position();
    sLightDir = glm::normalize(scene->env.lightDir);
    sVpW = w; sVpH = h;

    scene->pickFbo.Clear();
    glBindFramebuffer(GL_FRAMEBUFFER, scene->fbo.Handle());
    glViewport(0, 0, w, h);

    // Reset per-scene state
    scene->nextPickId     = 0;
    scene->activePickId     = 0;
    scene->pickIdOverride = 0;
    scene->pickEnabled    = true;
    scene->pickConsumed   = false;
    scene->meshFrameReady = false;

    scene->emissive          = false;
    scene->glow              = false;
    scene->glowRadius = 0.f;
    scene->numPointLights    = 0;

    // Clear but keep capacity (avoid realloc every frame)
    scene->drawList.clear();
    scene->vboAccum.clear();
    scene->lineBatch.clear();
    scene->pointBatch.clear();
    scene->textBatch.clear();
    // Reserve based on previous frame to avoid growth during draw
    scene->vboAccum.reserve(scene->stats.vertices + 256);
    scene->lineBatch.reserve(scene->stats.lineSegments * 6 + 64);
    scene->pointBatch.reserve(scene->stats.points + 32);

    scene->stats = {};
    scene->stats.viewportW    = w;
    scene->stats.viewportH    = h;
    scene->stats.msaaSamples  = scene->fbo.Samples();

    scene->matStack.resize(1);
    scene->frameMat = FrameMat(cfg.frame);
    scene->matStack[0] = glm::mat4(scene->frameMat);
}

// ── DrawSun / DrawGrid ───────────────────────────────────────────────

static void DrawSun() {
    ctx().pickEnabled = false;
    auto& env = ctx().env;

    // Sun follows camera — always in the sky, unreachable
    constexpr float kSunDist   = 30.f;
    constexpr float kSunRadius = 0.5f;
    glm::vec3 sunPos = sCamPos + glm::normalize(env.lightDir) * kSunDist;

    PushMatrix();
    ResetMatrix();
    glDepthFunc(GL_ALWAYS);  // render behind all geometry
    SetNextEmissive();
    Sphere(sunPos, kSunRadius, {1.f, .98f, .85f, 1.f}, 24);
    glDepthFunc(GL_LESS);
    PopMatrix();

    ctx().pickEnabled = true;
}

static void DrawGrid(const GridConfig& cfg, float camDist) {
    sGridShader.Use();
    sGridShader.Set("uInvViewProj", glm::inverse(sViewProj));
    sGridShader.Set("uViewProj", sViewProj);
    sGridShader.Set("uCamPos", sCamPos);
    sGridShader.Set("uCamDist", camDist);
    sGridShader.Set("uScaleFine",   cfg.scaleFine);
    sGridShader.Set("uScaleMedium", cfg.scaleMedium);
    sGridShader.Set("uScaleCoarse", cfg.scaleCoarse);
    sGridShader.Set("uColorFine",   cfg.colorFine);
    sGridShader.Set("uColorMedium", cfg.colorMedium);
    sGridShader.Set("uColorCoarse", cfg.colorCoarse);
    sGridShader.Set("uAxisXColor",  cfg.axisXColor);
    sGridShader.Set("uAxisYColor",  cfg.axisYColor);
    sGridShader.Set("uAxisThickness",    cfg.axisThickness);
    sGridShader.Set("uAxisScaleWithCam", cfg.axisScaleWithCam ? 1 : 0);
    sGridShader.Set("uFadeStart", cfg.fadeStart);
    sGridShader.Set("uFadeEnd",   cfg.fadeEnd);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sGridVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_CULL_FACE);
    ++ctx().stats.drawCalls;
}

// ── End ──────────────────────────────────────────────────────────────

void End() {
    if (sFrame.fly) {
        ctx().pickEnabled = false;
        auto& cam = ctx().cam;
        auto pivot = cam.Pivot();
        float s = cam.Distance() * 0.03f; // crosshair scale relative to distance
        Line(pivot, pivot+glm::vec3(s,0,0), {.95f,.25f,.25f,.7f}, 2.f);
        Line(pivot, pivot+glm::vec3(0,s,0), {.35f,.85f,.35f,.7f}, 2.f);
        Line(pivot, pivot+glm::vec3(0,0,s), {.35f,.50f,.95f,.7f}, 2.f);
        Line(cam.Eye(), pivot, {1,1,1,.15f}, 1.f);
        Text(pivot+glm::vec3(s*.5f,s*.5f,s), {1,1,1,.6f}, "%.1f", cam.Distance());
        ctx().pickEnabled = true;
    }

    if (ctx().env.showSun) DrawSun();

    ctx().stats.pointLights = ctx().numPointLights;

    auto& dl = ctx().drawList;

    if (!dl.empty()) {
        UploadVbo(sMeshVbo, sMeshVboCap, ctx().vboAccum.data(),
                  GLsizeiptr(ctx().vboAccum.size() * sizeof(MeshVert)));
        glBindVertexArray(sMeshVao);

        // Pick pass (depth-tested — front object wins at each pixel)
        if (ctx().pickEnabled) {
            BeginPickPass();
            sPickMeshShader.Use();
            sPickMeshShader.Set("uViewProj", sViewProj);
            for (auto& d : dl) {
                if (!d.pickId || d.shadingMode == 3) continue;
                sPickMeshShader.Set("uPickId", d.pickId);
                glDrawArrays(GL_TRIANGLES, d.offset, d.count);
                ++ctx().stats.pickDrawCalls;
            }
            EndPickPass();
        }

        SetMeshFrameUniforms();
        sMeshShader.Use();

        // Solid pass first (populates depth buffer)
        for (auto& d : dl) {
            if (d.shadingMode == 3) continue;
            sMeshShader.Set("uColor", d.color);
            sMeshShader.Set("uUnlit", d.shadingMode);
            glDrawArrays(GL_TRIANGLES, d.offset, d.count);
            ++ctx().stats.drawCalls;
        }

        // Glow pass last (additive on top of solid, depth-tested but no depth write)
        bool hadGlow = false;
        for (auto& d : dl) {
            if (d.shadingMode != 3) continue;
            if (!hadGlow) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                hadGlow = true;
            }
            sMeshShader.Set("uColor", d.color);
            sMeshShader.Set("uUnlit", 3);
            glDrawArrays(GL_TRIANGLES, d.offset, d.count);
            ++ctx().stats.drawCalls;
        }
        if (hadGlow) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }

        dl.clear();
        ctx().vboAccum.clear();
    }
    FlushPoints();
    FlushLines();
    if (ctx().gridCfg.enabled) DrawGrid(ctx().gridCfg, ctx().cam.Distance());

    // Async pick readback: read PREVIOUS frame's result (non-blocking),
    // then start THIS frame's read (GPU processes while CPU continues).
    if (sFrame.hovered && !ctx().pickConsumed) {
        ctx().hoveredPickId = ctx().pickFbo.FinishAsyncRead();
        auto& io = ImGui::GetIO();
        int mx = static_cast<int>(io.MousePos.x - sFrame.cx);
        int my = static_cast<int>(io.MousePos.y - sFrame.cy);
        ctx().pickFbo.BeginAsyncRead(mx, my);
        ctx().pickFbo.pboIdx = 1 - ctx().pickFbo.pboIdx;
        ctx().pickFbo.pboReady = true;
    }

    // Clear drag for released buttons (after user code already checked Released())
    for (int b = 0; b < kButtonCount; ++b)
        if (ctx().dragPickId[b] && !ImGui::IsMouseDown(b))
            ctx().dragPickId[b] = 0;

    ctx().fbo.Resolve();
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(ctx().fbo.Texture())),
                 {sFrame.w, sFrame.h}, {0, 1}, {1, 0});
    FlushText();
}

// ── coordinate frame ────────────────────────────────────────────────

void SetFrame(FrameId id)         { SetFrame(FrameMat(id)); }
void SetFrame(const glm::mat3& m) {
    // Right-multiply basis change: preserves world position, relabels local axes
    ctx().matStack.back() *= glm::mat4(glm::transpose(ctx().frameMat) * m);
    ctx().frameMat = m;
}
const glm::mat3& GetFrame() { return ctx().frameMat; }

// ── transform stack ──────────────────────────────────────────────────

void PushMatrix()  { auto& stk = ctx().matStack; stk.push_back(stk.back()); }
void PopMatrix()   { auto& stk = ctx().matStack; if (stk.size() > 1) stk.pop_back(); }
void ResetMatrix() { ctx().matStack.back() = glm::mat4(ctx().frameMat); }
void SetMatrix(const glm::mat4& m)  { ctx().matStack.back() = m; }
void Transform(const glm::mat4& m)  { ctx().matStack.back() *= m; }
void Translate(const glm::vec3& v)  { auto& m = ctx().matStack.back(); m = glm::translate(m, v); }
void Translate(float x, float y, float z) { Translate({x, y, z}); }
void Rotate(float deg, const glm::vec3& axis) { auto& m = ctx().matStack.back(); m = glm::rotate(m, glm::radians(deg), axis); }
void Rotate(const glm::quat& q) { ctx().matStack.back() *= glm::mat4_cast(q); }
void RotateX(float deg) { Rotate(deg, {1, 0, 0}); }
void RotateY(float deg) { Rotate(deg, {0, 1, 0}); }
void RotateZ(float deg) { Rotate(deg, {0, 0, 1}); }
void Scale(const glm::vec3& s) { auto& m = ctx().matStack.back(); m = glm::scale(m, s); }
void Scale(float s) { Scale({s, s, s}); }

} // namespace Kilo::Render

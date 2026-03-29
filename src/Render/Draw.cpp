#include "Render/DrawState.hpp"
#include "Render/DrawGlobe.hpp"
#include "Core/Log.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
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
    uint32_t zero = 0;
    for (auto p : pbo)
        glNamedBufferStorage(p, sizeof(uint32_t), &zero, GL_MAP_READ_BIT);
    pboIdx = 0;
    pboReady = false;
    if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::Render().error("PickFbo incomplete ({}x{})", w, h);
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
    uint32_t id = 0;
    if (ptr) { id = *ptr; glUnmapNamedBuffer(pbo[readIdx]); }
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
static std::unordered_map<int, IndexedMesh> sDiskCache;
static IndexedMesh sBoxCache;

template <typename GenT>
static void BuildCache(IndexedMesh& out, const GenT& gen) {
    float maxR2 = 0.f;
    for (auto it = gen.vertices(); !it.done(); it.next()) {
        auto v = it.generate();
        auto p = glm::vec3(v.position);
        out.pos.push_back(p);
        out.nrm.push_back(glm::vec3(v.normal));
        out.uv.push_back(glm::vec2(v.texCoord));
        maxR2 = glm::max(maxR2, glm::dot(p, p));
    }
    out.boundingRadius = std::sqrt(maxR2);
    for (auto it = gen.triangles(); !it.done(); it.next()) {
        auto t = it.generate();
        out.tri.push_back({t.vertices[0], t.vertices[1], t.vertices[2]});
    }
}

IndexedMesh& GetUnitSphere(int seg) {
    auto& m = sSphereCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::SphereMesh(1.f, seg, seg / 2));
    return m;
}

IndexedMesh& GetUnitBox() {
    if (sBoxCache.pos.empty()) BuildCache(sBoxCache, generator::BoxMesh({.5f, .5f, .5f}, {1, 1, 1}));
    return sBoxCache;
}

IndexedMesh& GetUnitCylinder(int seg) {
    auto& m = sCylinderCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CappedCylinderMesh(1.f, 1.f, seg, 1, 1));
    return m;
}

IndexedMesh& GetUnitCone(int seg) {
    auto& m = sConeCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CappedConeMesh(1.f, 1.f, seg, 1, 1));
    return m;
}

IndexedMesh& GetUnitCapsule(int seg) {
    auto& m = sCapsuleCache[seg];
    if (m.pos.empty()) BuildCache(m, generator::CapsuleMesh(1.f, 1.f, seg, 1, seg / 2));
    return m;
}

IndexedMesh& GetUnitDisk(int seg) {
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
    sMeshShader.Set("uCamPos", glm::vec3(0.f));  // camera-relative: camera at origin
    sMeshShader.Set("uBgColor", env.bgColor);
    sMeshShader.Set("uAmbient", env.ambient);
    sMeshShader.Set("uDiffuse", env.diffuse);
    sMeshShader.Set("uRoughness", env.roughness);
    sMeshShader.Set("uSpecular", env.specular);
    sMeshShader.Set("uFresnel", env.fresnel);
    sMeshShader.Set("uFogDensity", env.fogDensity);
    sMeshShader.Set("uFogStart", env.fogStart);
    sMeshShader.Set("uFogEnd", env.fogEnd);
    sMeshShader.Set("uFarPlane", sFarPlane);
    glUniform1i(sNumLightsLoc, ctx().numPointLights);
    for (int i = 0; i < ctx().numPointLights; ++i) {
        auto& light = ctx().pointLights[i];
        auto camRelPos = glm::vec3(glm::dvec3(light.pos) - sCamPosD);  // camera-relative
        glUniform3fv(sLightLocs[i].pos,   1, &camRelPos.x);
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
    // Scissor: only rasterize near cursor (GPU rejects all other fragments)
    auto& io = ImGui::GetIO();
    int mx = static_cast<int>(io.MousePos.x - sFrame.cx);
    int my = sVpH - static_cast<int>(io.MousePos.y - sFrame.cy); // flip Y for GL
    constexpr int kPickRadius = 4;
    glEnable(GL_SCISSOR_TEST);
    glScissor(mx - kPickRadius, my - kPickRadius, kPickRadius * 2, kPickRadius * 2);
}

static void EndPickPass() {
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx().fbo.Handle());
    glViewport(0, 0, sVpW, sVpH);
}

static void UploadGpuMesh(IndexedMesh& mesh);
static void ConsumeEmissive(const glm::vec3& centroid, float autoRadius);

void UploadMesh(const std::vector<MeshVert>& v) {
    auto& s = ctx();
    auto count  = static_cast<GLsizei>(v.size());
    auto offset = static_cast<GLsizei>(s.vboAccum.size());
    s.vboAccum.insert(s.vboAccum.end(), v.begin(), v.end());
    s.drawList.push_back({offset, count, s.currentColor, s.currentShadingMode, s.activePickId});
    s.stats.vertices += count;

    // Emissive glow: compute centroid + radius from flat vertices
    if (s.emissive && count > 0) {
        glm::vec3 centroid(0.f);
        for (GLsizei i = offset; i < offset + count; ++i)
            centroid += s.vboAccum[i].pos;
        centroid /= static_cast<float>(count);
        float maxR2 = 0.f;
        for (GLsizei i = offset; i < offset + count; ++i) {
            auto d = s.vboAccum[i].pos - centroid;
            maxR2 = std::max(maxR2, glm::dot(d, d));
        }
        ConsumeEmissive(centroid, std::sqrt(maxR2));
    } else {
        s.emissive = false; s.glow = false; s.glowRadius = 0.f;
    }
}

// Emit a glow sphere centered at `centroid` with given radius, using the current color.
static void EmitGlow(const glm::vec3& centroid, float glowR) {
    auto& s = ctx();
    auto glowModel = glm::scale(glm::translate(glm::mat4(1.f), centroid), glm::vec3(glowR));
    auto& glowMesh = GetUnitSphere(16);
    UploadGpuMesh(glowMesh);
    s.drawList.push_back({0, 0,
        {s.currentColor.r, s.currentColor.g, s.currentColor.b, kGlowAlpha},
        3, 0, &glowMesh.gpu, glowModel, glm::mat3(1.f), glowR});
    s.stats.vertices += glowMesh.gpu.indexCount;
}

// Consume one-shot emissive/glow flags. If emissive, emit a glow sphere.
// `centroid` and `autoRadius` are used when glowRadius was not explicitly set.
static void ConsumeEmissive(const glm::vec3& centroid, float autoRadius) {
    auto& s = ctx();
    bool emissive = s.emissive;
    float glowR   = s.glowRadius;
    s.emissive = false;
    s.glow     = false;
    s.glowRadius = 0.f;
    if (emissive) {
        if (glowR <= 0.f) glowR = std::max(autoRadius * kGlowRadiusScale, kGlowRadiusMin);
        EmitGlow(centroid, glowR);
    }
}

// ── GPU mesh upload (static, immutable — once per unique mesh) ────────

static void UploadGpuMesh(IndexedMesh& mesh) {
    auto& g = mesh.gpu;
    if (g.vao) return;  // already uploaded

    // Interleaved vertex buffer from indexed data
    std::vector<MeshVert> verts(mesh.pos.size());
    bool hasUV = !mesh.uv.empty();
    for (size_t i = 0; i < mesh.pos.size(); ++i) {
        verts[i].pos    = mesh.pos[i];
        verts[i].normal = mesh.nrm[i];
        if (hasUV) verts[i].uv = mesh.uv[i];
    }

    // Flatten triangle indices
    std::vector<uint32_t> indices;
    indices.reserve(mesh.tri.size() * 3);
    for (auto& t : mesh.tri) {
        indices.push_back(static_cast<uint32_t>(t[0]));
        indices.push_back(static_cast<uint32_t>(t[1]));
        indices.push_back(static_cast<uint32_t>(t[2]));
    }
    g.indexCount = static_cast<GLsizei>(indices.size());

    glCreateVertexArrays(1, &g.vao);
    glCreateBuffers(1, &g.vbo);
    glCreateBuffers(1, &g.ebo);
    glNamedBufferStorage(g.vbo, GLsizeiptr(verts.size() * sizeof(MeshVert)), verts.data(), 0);
    glNamedBufferStorage(g.ebo, GLsizeiptr(indices.size() * sizeof(uint32_t)), indices.data(), 0);

    // Same attribute layout as sMeshVao
    glVertexArrayVertexBuffer(g.vao, 0, g.vbo, 0, sizeof(MeshVert));
    glVertexArrayElementBuffer(g.vao, g.ebo);
    auto attr = [&](GLuint idx, GLint size, GLuint offset) {
        glEnableVertexArrayAttrib(g.vao, idx);
        glVertexArrayAttribFormat(g.vao, idx, size, GL_FLOAT, GL_FALSE, offset);
        glVertexArrayAttribBinding(g.vao, idx, 0);
    };
    attr(0, 3, offsetof(MeshVert, pos));
    attr(1, 3, offsetof(MeshVert, normal));
    attr(2, 2, offsetof(MeshVert, uv));
}

void UploadGpuDraw(IndexedMesh& mesh, const glm::mat4& model) {
    UploadGpuMesh(mesh);
    auto& s = ctx();
    // Camera-relative model: subtract camera position in double, cast result to float32.
    // Preserves sub-mm precision even at 200km+ altitude.
    glm::mat4 camRelModel = model;
    camRelModel[3] = glm::vec4(glm::vec3(glm::dvec3(model[3]) - sCamPosD), model[3].w);
    auto nmat = NormalMatrix(camRelModel);
    float maxScale = std::max({glm::length(glm::vec3(camRelModel[0])),
                               glm::length(glm::vec3(camRelModel[1])),
                               glm::length(glm::vec3(camRelModel[2]))});
    s.drawList.push_back({0, 0, s.currentColor, s.currentShadingMode,
                          s.activePickId, &mesh.gpu, camRelModel, nmat,
                          mesh.boundingRadius * maxScale, s.twoSided});
    s.stats.vertices += mesh.gpu.indexCount;

    // Emissive glow: centroid from model translation, radius from bounding sphere × scale
    float autoR = mesh.boundingRadius * maxScale;
    s.twoSided = false;
    ConsumeEmissive(glm::vec3(camRelModel[3]), autoR);
}

// ── line batching ────────────────────────────────────────────────────

void FlushLines() {
    if (ctx().lineBatch.empty()) return;
    auto count = static_cast<GLsizei>(ctx().lineBatch.size());
    UploadVbo(sLineVbo, sLineVboCap, ctx().lineBatch.data(), GLsizeiptr(count * sizeof(LineVert)));
    ++ctx().stats.drawCalls;
    ctx().stats.lineSegments += count / 6; // 6 verts = 2 triangles per line segment

    sLineShader.Use();
    sLineShader.Set("uViewProj", sViewProj);
    sLineShader.Set("uProjScale", sProj[1][1] * float(sVpH) * 0.005f);
    sLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    sLineShader.Set("uFarPlane", sFarPlane);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sLineVao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);

    if (ctx().pickEnabled) {
        BeginPickPass();
        sPickLineShader.Use();
        sPickLineShader.Set("uViewProj", sViewProj);
        sPickLineShader.Set("uProjScale", sProj[1][1] * float(sVpH) * 0.005f);
        sPickLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        sPickLineShader.Set("uFarPlane", sFarPlane);
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
    uint32_t pid = ctx().activePickId;
    auto& batch = ctx().lineBatch;
    batch.push_back({a, b, {-1, 0}, ca, pid, width});
    batch.push_back({a, b, { 1, 0}, ca, pid, width});
    batch.push_back({a, b, { 1, 1}, cb, pid, width});
    batch.push_back({a, b, {-1, 0}, ca, pid, width});
    batch.push_back({a, b, { 1, 1}, cb, pid, width});
    batch.push_back({a, b, {-1, 1}, cb, pid, width});
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
    sPointShader.Set("uFarPlane", sFarPlane);

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
        sPickPointShader.Set("uFarPlane", sFarPlane);
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

Group::Group() : owned_(ctx().pickIdOverride == 0) {
    if (owned_) { ctx().pickIdOverride = ++ctx().nextPickId; ctx().activePickId = ctx().pickIdOverride; }
}
Group::~Group() { if (owned_) ctx().pickIdOverride = 0; }

// ── text overlay ─────────────────────────────────────────────────────

static void FlushText() {
    if (ctx().textBatch.empty()) return;
    auto* dl = ImGui::GetWindowDrawList();
    ImGui::PushClipRect({sFrame.cx, sFrame.cy}, {sFrame.cx + sFrame.w, sFrame.cy + sFrame.h}, true);
    for (auto& e : ctx().textBatch) {
        float tx, ty;
        if (e.screenPos.x >= 0.f) {
            tx = e.screenPos.x; ty = e.screenPos.y;
        } else {
            auto screen = WorldToScreen(e.worldPos);
            if (screen.x < 0.f) continue;
            tx = screen.x; ty = screen.y;
            if (e.centered) {
                auto sz = ImGui::CalcTextSize(e.text.c_str());
                tx -= sz.x * 0.5f; ty -= sz.y * 0.5f;
            }
        }
        dl->AddText({tx, ty},
            ImGui::ColorConvertFloat4ToU32({e.color.r, e.color.g, e.color.b, e.color.a}),
            e.text.c_str());
    }
    ImGui::PopClipRect();
    ctx().textBatch.clear();
}

// ── Init ─────────────────────────────────────────────────────────────

void Init(const std::string& dir, int msaaSamples) {
    sMsaaSamples = msaaSamples;
    sMeshShader      = Shader(dir + "/Basic.vert",     dir + "/Basic.frag");
    sLineShader      = Shader(dir + "/Line.vert",      dir + "/Line.frag");
    sGridShader      = Shader(dir + "/Grid.vert",      dir + "/Grid.frag");
    sPointShader     = Shader(dir + "/Point.vert",     dir + "/Point.frag");
    sPickMeshShader  = Shader(dir + "/Pick.vert",      dir + "/Pick.frag");
    sPickLineShader  = Shader(dir + "/PickLine.vert",  dir + "/PickLine.frag");
    sPickPointShader = Shader(dir + "/PickPoint.vert", dir + "/PickPoint.frag");
    InitGlobe(dir);

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
        {2, {2, offsetof(LineVert, expand)}}, {3, {4, offsetof(LineVert, color)}},
        {5, {1, offsetof(LineVert, width)}}});
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

    // One-time GL state (invariant across frames)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_SCISSOR_TEST);
}

// ── Shutdown ─────────────────────────────────────────────────────────

void Shutdown() {
    ShutdownModels();
    sScenes.clear();
    ShutdownGlobe();
    sMeshShader = {}; sLineShader = {}; sGridShader = {}; sPointShader = {};
    sPickMeshShader = {}; sPickLineShader = {}; sPickPointShader = {};
    GLuint vaos[] = {sMeshVao, sLineVao, sGridVao, sPointVao};
    GLuint vbos[] = {sMeshVbo, sLineVbo, sPointVbo};
    glDeleteVertexArrays(4, vaos);
    glDeleteBuffers(3, vbos);
    sMeshVao = sLineVao = sGridVao = sPointVao = 0;
    sMeshVbo = sLineVbo = sPointVbo = 0;
    sMeshVboCap = sLineVboCap = sPointVboCap = 0;
    auto destroyGpuCache = [](auto& cache) {
        for (auto& [_, m] : cache) {
            if (m.gpu.vao) { glDeleteVertexArrays(1, &m.gpu.vao);
                             glDeleteBuffers(1, &m.gpu.vbo);
                             glDeleteBuffers(1, &m.gpu.ebo); }
        }
        cache.clear();
    };
    destroyGpuCache(sSphereCache); destroyGpuCache(sCylinderCache);
    destroyGpuCache(sConeCache); destroyGpuCache(sCapsuleCache);
    destroyGpuCache(sDiskCache);
    if (sBoxCache.gpu.vao) { glDeleteVertexArrays(1, &sBoxCache.gpu.vao);
                              glDeleteBuffers(1, &sBoxCache.gpu.vbo);
                              glDeleteBuffers(1, &sBoxCache.gpu.ebo); }
    sBoxCache = {};
    sFrame = {};
}

// ── scene getters ────────────────────────────────────────────────────

// GetScene defined in DrawState.hpp (shared with DrawGlobe.cpp)

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
    scene->fbo.Resize(w, h, sMsaaSamples);
    scene->pickFbo.Resize(w, h);
    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

    auto& io = ImGui::GetIO();
    auto& cam = scene->cam;
    bool hovered = ImGui::IsItemHovered();
    if (!hovered) { scene->hoveredPickId = 0; scene->pickFbo.pboReady = false; }
    bool active = ImGui::IsItemActive();
    bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    bool fly = active && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !cam.Following();

    if (hovered && io.MouseWheel != 0.f) cam.Zoom(io.MouseWheel);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        if (shift) cam.Pan(io.MouseDelta.x, io.MouseDelta.y);
        else       cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    }

    { // Fly mode cursor lock (per-scene — prevents one panel from locking cursor for all scenes)
        auto* win = glfwGetCurrentContext();
        if (fly && !scene->flyLocked) { glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED); scene->flyLocked = true; }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && scene->flyLocked) { glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL); scene->flyLocked = false; }
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
    // Auto far plane: scales with orbit distance + limb distance when Globe active
    float autoFar = float(std::max(10000.0, cam.Distance() * 100.0));
    if (scene->geoRef.valid) {
        double camAlt = std::max(0.0, cam.Eye().z);
        double limb   = std::sqrt(2.0 * GeoRef::a * camAlt + camAlt * camAlt);
        autoFar = float(std::max(double(autoFar), std::max(GeoRef::a * 2.0, limb * 1.1)));
    }
    sFarPlane = cam.FarPlane() > 0.f ? cam.FarPlane() : autoFar;
    sProj = cam.Projection(aspect, autoFar);
    sCamPosD     = cam.Eye();
    sCamPos      = glm::vec3(sCamPosD);
    // Camera-relative rendering: view matrix with camera at origin.
    // Model translations are shifted by -camPos (in double) before upload.
    // This eliminates float32 jitter at large world coordinates (e.g. 200km altitude).
    sView        = cam.ViewCamRelative();
    sViewProj    = sProj * sView;
    sInvViewProj = glm::inverse(sViewProj);
    sFrustum     = ExtractFrustum(sViewProj);
    sLightDir = glm::normalize(scene->env.lightDir);
    sVpW = w; sVpH = h;

    if (hovered) scene->pickFbo.Clear();
    glBindFramebuffer(GL_FRAMEBUFFER, scene->fbo.Handle());
    glViewport(0, 0, w, h);

    // Reset per-scene state
    scene->nextPickId     = 0;
    scene->activePickId     = 0;
    scene->pickIdOverride = 0;
    scene->pickEnabled    = hovered;
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
    scene->frameMat = glm::mat3(1.f);
    scene->matStack[0] = glm::mat4(1.f);
}

// ── DrawSun / DrawGrid ───────────────────────────────────────────────

static void DrawSun() {
    auto& env = ctx().env;

    constexpr float kSunDist   = 30.f;
    constexpr float kSunRadius = 0.5f;
    glm::vec3 sunPos = sCamPos + glm::normalize(env.lightDir) * kSunDist;

    glDepthFunc(GL_ALWAYS);
    SetNextEmissive();
    Sphere(sunPos, kSunRadius, {1.f, .98f, .85f, 1.f}, 24);
    glDepthFunc(GL_LESS);
}

static glm::vec4 FrameAxisColor(const glm::mat3& fm, int axis, const glm::vec4 (&colors)[3]) {
    int best = 0;
    for (int u = 1; u < 3; ++u)
        if (std::abs(fm[u][axis]) > std::abs(fm[best][axis])) best = u;
    return colors[best];
}

static void DrawGrid(const GridConfig& cfg, float camDist) {
    const glm::vec4 axisColors[3] = {cfg.axisXColor, cfg.axisYColor, {.35f,.50f,.95f,1.f}};
    const glm::mat3& fm = ctx().frameMat;

    sGridShader.Use();
    sGridShader.Set("uInvViewProj", sInvViewProj);
    sGridShader.Set("uViewProj", sViewProj);
    sGridShader.Set("uCamPos", sCamPos);  // world position for grid pattern (not camera-relative)
    sGridShader.Set("uCamDist", camDist);
    sGridShader.Set("uScaleFine",   cfg.scaleFine);
    sGridShader.Set("uScaleMedium", cfg.scaleMedium);
    sGridShader.Set("uScaleCoarse", cfg.scaleCoarse);
    sGridShader.Set("uColorFine",   cfg.colorFine);
    sGridShader.Set("uColorMedium", cfg.colorMedium);
    sGridShader.Set("uColorCoarse", cfg.colorCoarse);
    sGridShader.Set("uAxisXColor",  FrameAxisColor(fm, 0, axisColors));
    sGridShader.Set("uAxisYColor",  FrameAxisColor(fm, 1, axisColors));
    sGridShader.Set("uAxisThickness",    cfg.axisThickness);
    sGridShader.Set("uAxisScaleWithCam", cfg.axisScaleWithCam ? 1 : 0);
    sGridShader.Set("uFadeStart", cfg.fadeStart);
    sGridShader.Set("uFadeEnd",   cfg.fadeEnd);
    sGridShader.Set("uFarPlane",  sFarPlane);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sGridVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_CULL_FACE);
    ++ctx().stats.drawCalls;
}

// ── End (sub-steps) ─────────────────────────────────────────────────

static void SubmitMeshes() {
    auto& dl = ctx().drawList;
    if (dl.empty()) return;

    // Upload flat (CPU-transformed) vertices
    if (!ctx().vboAccum.empty())
        UploadVbo(sMeshVbo, sMeshVboCap, ctx().vboAccum.data(),
                  GLsizeiptr(ctx().vboAccum.size() * sizeof(MeshVert)));

    // VAO tracking: skip redundant binds
    GLuint boundVao = 0;
    auto bindAndDraw = [&](const MeshDraw& d) {
        if (d.gpuMesh) {
            if (d.gpuMesh->vao != boundVao) { glBindVertexArray(d.gpuMesh->vao); boundVao = d.gpuMesh->vao; }
            glDrawElements(GL_TRIANGLES, d.gpuMesh->indexCount, GL_UNSIGNED_INT, nullptr);
        } else {
            if (sMeshVao != boundVao) { glBindVertexArray(sMeshVao); boundVao = sMeshVao; }
            glDrawArrays(GL_TRIANGLES, d.offset, d.count);
        }
    };

    // Pick pass — scissored to cursor, skipped if no pickable objects
    bool anyPickable = ctx().pickEnabled && std::any_of(dl.begin(), dl.end(),
        [](const MeshDraw& d) { return d.pickId != 0 && d.shadingMode != 3; });
    if (anyPickable) {
        BeginPickPass();
        sPickMeshShader.Use();
        sPickMeshShader.Set("uViewProj", sViewProj);
        sPickMeshShader.Set("uFarPlane", sFarPlane);
        for (auto& d : dl) {
            if (!d.pickId || d.shadingMode == 3) continue;
            sPickMeshShader.Set("uPickId", d.pickId);
            sPickMeshShader.Set("uModel", d.model);
            bindAndDraw(d);
            ++ctx().stats.pickDrawCalls;
        }
        EndPickPass();
    }

    // Sort: solid before glow, group by mesh pointer (reduces VAO switches)
    std::stable_sort(dl.begin(), dl.end(), [](const MeshDraw& a, const MeshDraw& b) {
        bool aGlow = a.shadingMode == 3, bGlow = b.shadingMode == 3;
        if (aGlow != bGlow) return bGlow;
        return a.gpuMesh < b.gpuMesh;
    });

    // Main color pass
    SetMeshFrameUniforms();
    sMeshShader.Use();
    boundVao = 0;
    bool inGlow = false;
    for (auto& d : dl) {
        if (d.gpuMesh && !InsideFrustum(sFrustum, glm::vec3(d.model[3]), d.worldRadius))
            continue;
        if (!inGlow && d.shadingMode == 3) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            inGlow = true;
        }
        if (d.twoSided) glDisable(GL_CULL_FACE);
        sMeshShader.Set("uColor", d.color);
        sMeshShader.Set("uUnlit", d.shadingMode);
        sMeshShader.Set("uModel", d.model);
        sMeshShader.Set("uNormalMat", d.normalMat);
        bindAndDraw(d);
        if (d.twoSided) glEnable(GL_CULL_FACE);
        ++ctx().stats.drawCalls;
    }
    if (inGlow) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }

    dl.clear();
    ctx().vboAccum.clear();
}

static void ProcessPicking() {
    if (!sFrame.hovered || ctx().pickConsumed) return;
    ctx().hoveredPickId = ctx().pickFbo.FinishAsyncRead();
    auto& io = ImGui::GetIO();
    int mx = static_cast<int>(io.MousePos.x - sFrame.cx);
    int my = static_cast<int>(io.MousePos.y - sFrame.cy);
    ctx().pickFbo.BeginAsyncRead(mx, my);
    ctx().pickFbo.pboIdx = 1 - ctx().pickFbo.pboIdx;
    ctx().pickFbo.pboReady = true;
}

static void ResolveAndPresent() {
    // Clear drag for released buttons
    for (int b = 0; b < kButtonCount; ++b)
        if (ctx().dragPickId[b] && !ImGui::IsMouseDown(b))
            ctx().dragPickId[b] = 0;

    ctx().fbo.Resolve();
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(ctx().fbo.Texture())),
                 {sFrame.w, sFrame.h}, {0, 1}, {1, 0});
    FlushText();
}

// ── End ──────────────────────────────────────────────────────────────

void End() {
    // Internal draws must not inherit pick IDs from user code
    ctx().activePickId = 0;

    // Crosshair and sun operate in internal coords — bypass frame
    PushMatrix();
    SetMatrix(glm::mat4(1.f));

    if (sFrame.fly) {
        auto& cam   = ctx().cam;
        glm::vec3 pivot = glm::vec3(cam.Target());
        float s     = cam.Distance() * 0.03f;
        const glm::mat3& fm = ctx().frameMat;
        const glm::vec4 axColors[3] = {
            {.95f,.25f,.25f,.7f}, {.35f,.85f,.35f,.7f}, {.35f,.50f,.95f,.7f}};
        Line(pivot, pivot + fm[0] * s, axColors[0], 2.f);
        Line(pivot, pivot + fm[1] * s, axColors[1], 2.f);
        Line(pivot, pivot + fm[2] * s, axColors[2], 2.f);
        Line(glm::vec3(cam.Eye()), pivot, {1,1,1,.15f}, 1.f);
        Text(pivot + fm[0]*s*.4f + fm[2]*s*.4f, {1,1,1,.6f}, "%.1f", cam.Distance());
    }

    if (ctx().env.showSun) DrawSun();

    PopMatrix();

    ctx().stats.pointLights = ctx().numPointLights;

    // Surface extensions (render first — globe/grid are the farthest geometry)
    if (ctx().globeCfg.enabled) DrawGlobe(ctx().globeCfg);
    if (ctx().gridCfg.enabled)  DrawGrid(ctx().gridCfg, float(ctx().cam.Distance()));

    SubmitMeshes();
    FlushPoints();
    FlushLines();
    ProcessPicking();
    ResolveAndPresent();
}

// ── coordinate frame ────────────────────────────────────────────────

void SetFrame(FrameId id)         { SetFrame(FrameMat(id)); }
void SetFrame(const glm::mat3& m) {
    ctx().matStack.back() *= glm::mat4(glm::transpose(ctx().frameMat) * m);
    ctx().frameMat = m;
    ctx().cam.SetFrame(m);
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
void TranslateGeo(double lat, double lon, double alt) {
    // Full double pipeline: geodetic → ECEF → ENU → frame → camera-relative → float32
    // No float32 truncation until the final cast — sub-mm precision at any distance.
    auto& gr = ctx().geoRef;
    auto enu = gr.ToInternal(lat, lon, alt);                        // double
    auto framed = glm::dvec3(glm::transpose(glm::dmat3(ctx().frameMat)) * enu);  // double
    auto camRel = framed - sCamPosD;                                 // double
    Translate(glm::vec3(camRel));                                    // float32 only here
}
void Rotate(float deg, const glm::vec3& axis) { auto& m = ctx().matStack.back(); m = glm::rotate(m, glm::radians(deg), axis); }
void Rotate(const glm::quat& q) { ctx().matStack.back() *= glm::mat4_cast(q); }
void RotateX(float deg) { Rotate(deg, {1, 0, 0}); }
void RotateY(float deg) { Rotate(deg, {0, 1, 0}); }
void RotateZ(float deg) { Rotate(deg, {0, 0, 1}); }
void Scale(const glm::vec3& s) { auto& m = ctx().matStack.back(); m = glm::scale(m, s); }
void Scale(float s) { Scale({s, s, s}); }

} // namespace Kilo::Render

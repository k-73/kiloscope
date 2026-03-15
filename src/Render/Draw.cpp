#include "Render/DrawState.hpp"
#include <GLFW/glfw3.h>
#include <generator/SphereMesh.hpp>
#include <generator/CappedCylinderMesh.hpp>
#include <generator/CappedConeMesh.hpp>

namespace Kilo::Render {

// ── FBO implementations ──────────────────────────────────────────────

void PickFbo::Resize(int nw, int nh) {
    if (nw == w && nh == h) return;
    Destroy(); w = nw; h = nh;
    glCreateFramebuffers(1, &fbo);
    glCreateTextures(GL_TEXTURE_2D, 1, &color);
    glTextureStorage2D(color, 1, GL_R32UI, w, h);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
    glCreateRenderbuffers(1, &depth);
    glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT32F, w, h);
    glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
}
void PickFbo::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, w, h); }
void PickFbo::Clear() {
    Bind(); GLuint zero = 0; glClearBufferuiv(GL_COLOR, 0, &zero);
    float one = 1.f; glClearBufferfv(GL_DEPTH, 0, &one);
}
uint32_t PickFbo::ReadPixel(int x, int y) const {
    if (x < 0 || x >= w || y < 0 || y >= h) return 0;
    uint32_t id = 0; glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id); return id;
}
void PickFbo::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (color) { glDeleteTextures(1, &color); color = 0; }
    if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
    w = h = 0;
}

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
    sMeshShader.Set("uNumPointLights", sNumPointLights);
    for (int i = 0; i < sNumPointLights; ++i) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "uPLPos[%d]",   i); sMeshShader.Set(buf, sPointLights[i].pos);
        std::snprintf(buf, sizeof(buf), "uPLColor[%d]", i); sMeshShader.Set(buf, sPointLights[i].color);
        std::snprintf(buf, sizeof(buf), "uPLRange[%d]", i); sMeshShader.Set(buf, sPointLights[i].range);
    }
    sMeshFrameReady = true;
}

void SetMeshUniforms(const glm::vec4& color, bool unlit) {
    sCurrentColor = color;
    sCurrentUnlitMode = sGlow ? 3 : (sEmissive ? 2 : (unlit ? 1 : 0));
}

// ── pick pass helpers ────────────────────────────────────────────────

static void BeginPickPass() { sFrame.scene->pickFbo.Bind(); glDepthMask(GL_TRUE); }
static void EndPickPass() { glBindFramebuffer(GL_FRAMEBUFFER, sFrame.scene->fbo.Handle()); glViewport(0, 0, sVpW, sVpH); }

void UploadMesh(const std::vector<MeshVert>& v) {
    auto count = static_cast<GLsizei>(v.size());
    auto offset = static_cast<GLsizei>(sVboAccum.size());
    sVboAccum.insert(sVboAccum.end(), v.begin(), v.end());
    sDrawList.push_back({offset, count, sCurrentColor, sCurrentUnlitMode, sLastPickId});
    sStats.vertices += count;

    bool wasEmissive = sEmissive;
    float glowR = sEmissiveGlowRadius;
    sEmissive = false;
    sGlow = false;
    sEmissiveGlowRadius = 0.f;

    if (wasEmissive && count > 0) {
        glm::vec3 centroid(0.f);
        for (GLsizei i = offset; i < offset + count; ++i) centroid += sVboAccum[i].pos;
        centroid /= static_cast<float>(count);

        if (glowR <= 0.f) {
            float maxR = 0.f;
            for (GLsizei i = offset; i < offset + count; ++i)
                maxR = glm::max(maxR, glm::length(sVboAccum[i].pos - centroid));
            glowR = glm::max(maxR * 2.f, 0.05f);
        }

        sMeshScratch.clear();
        AppendMesh(sMeshScratch, generator::SphereMesh(glowR, 16, 8),
                   glm::translate(glm::mat4(1.f), centroid));
        auto glowOffset = static_cast<GLsizei>(sVboAccum.size());
        auto glowCount  = static_cast<GLsizei>(sMeshScratch.size());
        sVboAccum.insert(sVboAccum.end(), sMeshScratch.begin(), sMeshScratch.end());
        sDrawList.push_back({glowOffset, glowCount,
            {sCurrentColor.r, sCurrentColor.g, sCurrentColor.b, 0.35f}, 3, 0});
        sStats.vertices += glowCount;
    }
}

// ── line batching ────────────────────────────────────────────────────

void FlushLines() {
    if (sLineBatch.empty()) return;
    auto count = static_cast<GLsizei>(sLineBatch.size());
    glNamedBufferData(sLineVbo, GLsizeiptr(count * sizeof(LineVert)), sLineBatch.data(), GL_DYNAMIC_DRAW);
    ++sStats.drawCalls; sStats.lineSegments += count / 6;

    sLineShader.Use();
    sLineShader.Set("uView", sView); sLineShader.Set("uProj", sProj);
    sLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    sLineShader.Set("uLineWidth", sLineWidth);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
    glBindVertexArray(sLineVao); glDrawArrays(GL_TRIANGLES, 0, count);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE);

    if (sPickEnabled) {
        BeginPickPass(); sPickLineShader.Use();
        sPickLineShader.Set("uView", sView); sPickLineShader.Set("uProj", sProj);
        sPickLineShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        sPickLineShader.Set("uLineWidth", sLineWidth);
        glDisable(GL_CULL_FACE); glBindVertexArray(sLineVao);
        glDrawArrays(GL_TRIANGLES, 0, count); glEnable(GL_CULL_FACE);
        ++sStats.pickDrawCalls; EndPickPass();
    }
    sLineBatch.clear();
}

void BatchLineGradient(const glm::vec3& a, const glm::vec3& b,
                        const glm::vec4& ca, const glm::vec4& cb, float width) {
    if (!sLineBatch.empty() && width != sLineWidth) FlushLines();
    sLineWidth = width;
    uint32_t pid = sLastPickId;
    sLineBatch.push_back({a, b, {-1, 0}, ca, pid}); sLineBatch.push_back({a, b, { 1, 0}, ca, pid});
    sLineBatch.push_back({a, b, { 1, 1}, cb, pid}); sLineBatch.push_back({a, b, {-1, 0}, ca, pid});
    sLineBatch.push_back({a, b, { 1, 1}, cb, pid}); sLineBatch.push_back({a, b, {-1, 1}, cb, pid});
}

void BatchLine(const glm::vec3& a, const glm::vec3& b,
               const glm::vec4& color, float width) {
    BatchLineGradient(a, b, color, color, width);
}

// ── point batching ───────────────────────────────────────────────────

void FlushPoints() {
    if (sPointBatch.empty()) return;
    auto count = static_cast<GLsizei>(sPointBatch.size());
    glNamedBufferData(sPointVbo, GLsizeiptr(count * sizeof(PointVert)), sPointBatch.data(), GL_DYNAMIC_DRAW);
    ++sStats.drawCalls; sStats.points += count;

    sPointShader.Use();
    sPointShader.Set("uView", sView); sPointShader.Set("uProj", sProj);
    sPointShader.Set("uPointSize", sPointSize); sPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
    glEnable(GL_PROGRAM_POINT_SIZE); glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
    glBindVertexArray(sPointVao); glDrawArrays(GL_POINTS, 0, count);
    glDisable(GL_PROGRAM_POINT_SIZE); glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE);

    if (sPickEnabled) {
        BeginPickPass(); sPickPointShader.Use();
        sPickPointShader.Set("uView", sView); sPickPointShader.Set("uProj", sProj);
        sPickPointShader.Set("uPointSize", sPointSize); sPickPointShader.Set("uViewportSize", glm::vec2(sVpW, sVpH));
        glEnable(GL_PROGRAM_POINT_SIZE); glDisable(GL_CULL_FACE);
        glBindVertexArray(sPointVao); glDrawArrays(GL_POINTS, 0, count);
        glDisable(GL_PROGRAM_POINT_SIZE); glEnable(GL_CULL_FACE);
        ++sStats.pickDrawCalls; EndPickPass();
    }
    sPointBatch.clear();
}

// ── projection + interaction ─────────────────────────────────────────

glm::vec2 WorldToScreen(const glm::vec3& worldPos) {
    glm::vec4 clip = sViewProj * glm::vec4(worldPos, 1.f);
    if (clip.w <= 0.f) return {-1.f, -1.f};
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return { sFrame.cx + (ndc.x * 0.5f + 0.5f) * sFrame.w,
             sFrame.cy + (1.f - (ndc.y * 0.5f + 0.5f)) * sFrame.h };
}

bool EventState::Clicked(int button) const { return hovered_ && ImGui::IsMouseClicked(button); }

EventState Event() {
    EventState state;
    state.hovered_ = sFrame.hovered && sLastPickId != 0
                  && sFrame.scene && sLastPickId == sFrame.scene->hoveredPickId;
    return state;
}

// ── text overlay ─────────────────────────────────────────────────────

static void FlushText() {
    if (sTextBatch.empty()) return;
    auto* dl = ImGui::GetWindowDrawList();
    ImGui::PushClipRect({sFrame.cx, sFrame.cy}, {sFrame.cx + sFrame.w, sFrame.cy + sFrame.h}, true);
    for (auto& e : sTextBatch) {
        auto screen = WorldToScreen(e.worldPos);
        if (screen.x < 0.f) continue;
        dl->AddText({screen.x, screen.y},
            ImGui::ColorConvertFloat4ToU32({e.color.r, e.color.g, e.color.b, e.color.a}),
            e.text.c_str());
    }
    ImGui::PopClipRect();
    sTextBatch.clear();
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
    glCreateVertexArrays(1, &sMeshVao); glCreateBuffers(1, &sMeshVbo);
    SetupVao(sMeshVao, sMeshVbo, sizeof(MeshVert), {
        {0, {3, offsetof(MeshVert, pos)}}, {1, {3, offsetof(MeshVert, normal)}}});

    glCreateVertexArrays(1, &sLineVao); glCreateBuffers(1, &sLineVbo);
    SetupVao(sLineVao, sLineVbo, sizeof(LineVert), {
        {0, {3, offsetof(LineVert, pos)}}, {1, {3, offsetof(LineVert, otherEnd)}},
        {2, {2, offsetof(LineVert, expand)}}, {3, {4, offsetof(LineVert, color)}}});
    glEnableVertexArrayAttrib(sLineVao, 4);
    glVertexArrayAttribIFormat(sLineVao, 4, 1, GL_UNSIGNED_INT, offsetof(LineVert, pickId));
    glVertexArrayAttribBinding(sLineVao, 4, 0);

    glCreateVertexArrays(1, &sGridVao);

    glCreateVertexArrays(1, &sPointVao); glCreateBuffers(1, &sPointVbo);
    SetupVao(sPointVao, sPointVbo, sizeof(PointVert), {
        {0, {3, offsetof(PointVert, pos)}}, {1, {4, offsetof(PointVert, color)}}});
    glEnableVertexArrayAttrib(sPointVao, 2);
    glVertexArrayAttribIFormat(sPointVao, 2, 1, GL_UNSIGNED_INT, offsetof(PointVert, pickId));
    glVertexArrayAttribBinding(sPointVao, 2, 0);
}

// ── scene getters ────────────────────────────────────────────────────

static SceneData& GetScene(uint32_t id) { auto& s = sScenes[id]; if (!s) s = std::make_unique<SceneData>(); return *s; }
static SceneData& GetScene(const char* name) { return GetScene(HashName(name)); }

Camera& GetCamera() { return sFrame.scene->cam; }
Camera& GetCamera(const char* name) { return GetScene(name).cam; }
Environment& GetEnvironment() { return sFrame.scene->env; }
Environment& GetEnvironment(const char* name) { return GetScene(name).env; }

int PointLight(const glm::vec3& pos, const glm::vec3& color, float range) {
    if (sNumPointLights >= kMaxPointLights) return -1;
    int idx = sNumPointLights++;
    sPointLights[idx] = {pos, color, range};
    return idx;
}

int GetPointLightCount() { return sNumPointLights; }
PointLightInfo* GetPointLights() { return sPointLights; }

void SetNextEmissive(float glowRadius) {
    sEmissive = true;
    sEmissiveGlowRadius = glowRadius;
}
void SetNextGlow() { sGlow = true; }

const Stats& GetStats() { return sStats; }

void Grid() { sFrame.scene->gridCfg.enabled = true; }
void Grid(const GridConfig& cfg) { sFrame.scene->gridCfg = cfg; }
GridConfig& GetGrid() { return sFrame.scene->gridCfg; }
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
    sEnv = &scene->env;

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    scene->fbo.Bind(sEnv->bgColor.r, sEnv->bgColor.g, sEnv->bgColor.b);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    float aspect = static_cast<float>(w) / std::max(1, h);
    sView = cam.View(); sProj = cam.Projection(aspect); sViewProj = sProj * sView;
    sCamPos = cam.Position(); sLightDir = glm::normalize(sEnv->lightDir);
    sVpW = w; sVpH = h;

    scene->pickFbo.Clear();
    glBindFramebuffer(GL_FRAMEBUFFER, scene->fbo.Handle());
    glViewport(0, 0, w, h);

    sNextPickId = 0; sLastPickId = 0; sPickIdOverride = 0;
    sPickEnabled = true; sMeshFrameReady = false;
    sEmissive = false; sGlow = false; sEmissiveGlowRadius = 0.f;
    sNumPointLights = 0;
    sDrawList.clear(); sVboAccum.clear();
    sStats = {}; sStats.viewportW = w; sStats.viewportH = h; sStats.msaaSamples = scene->fbo.Samples();
    sLineBatch.clear(); sPointBatch.clear(); sTextBatch.clear();
    sMatStack.resize(1); sMatStack[0] = glm::mat4(1.f);
}

// ── DrawSun / DrawGrid ───────────────────────────────────────────────

static void DrawSun() {
    sPickEnabled = false;
    glm::vec3 sunPos = glm::normalize(sEnv->lightDir) * sEnv->sunDistance;
    PushMatrix(); ResetMatrix();
    SetNextEmissive();
    Sphere(sunPos, sEnv->sunRadius, {1.f, .98f, .85f, 1.f}, 24);
    Line({0,0,0}, sunPos, {1,.95f,.7f,.12f}, 1.f);
    PopMatrix(); sPickEnabled = true;
}

static void DrawGrid(const GridConfig& cfg, float camDist) {
    sGridShader.Use();
    sGridShader.Set("uView", sView); sGridShader.Set("uProj", sProj);
    sGridShader.Set("uViewProj", sViewProj); sGridShader.Set("uCamPos", sCamPos);
    sGridShader.Set("uCamDist", camDist);
    sGridShader.Set("uScaleFine", cfg.scaleFine); sGridShader.Set("uScaleMedium", cfg.scaleMedium);
    sGridShader.Set("uScaleCoarse", cfg.scaleCoarse);
    sGridShader.Set("uColorFine", cfg.colorFine); sGridShader.Set("uColorMedium", cfg.colorMedium);
    sGridShader.Set("uColorCoarse", cfg.colorCoarse);
    sGridShader.Set("uAxisXColor", cfg.axisXColor); sGridShader.Set("uAxisYColor", cfg.axisYColor);
    sGridShader.Set("uAxisThickness", cfg.axisThickness);
    sGridShader.Set("uAxisScaleWithCam", cfg.axisScaleWithCam ? 1 : 0);
    sGridShader.Set("uFadeStart", cfg.fadeStart); sGridShader.Set("uFadeEnd", cfg.fadeEnd);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE);
    glBindVertexArray(sGridVao); glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glEnable(GL_CULL_FACE);
    ++sStats.drawCalls;
}

// ── End ──────────────────────────────────────────────────────────────

void End() {
    if (sFrame.fly) {
        sPickEnabled = false;
        auto& cam = sFrame.scene->cam;
        auto pivot = cam.Pivot(); float s = cam.Distance() * 0.03f;
        Line(pivot, pivot+glm::vec3(s,0,0), {.95f,.25f,.25f,.7f}, 2.f);
        Line(pivot, pivot+glm::vec3(0,s,0), {.35f,.85f,.35f,.7f}, 2.f);
        Line(pivot, pivot+glm::vec3(0,0,s), {.35f,.50f,.95f,.7f}, 2.f);
        Line(cam.Eye(), pivot, {1,1,1,.15f}, 1.f);
        Text(pivot+glm::vec3(s*.5f,s*.5f,s), {1,1,1,.6f}, "%.1f", cam.Distance());
        sPickEnabled = true;
    }

    if (sEnv->showSun) DrawSun();

    sStats.pointLights = sNumPointLights;

    if (!sDrawList.empty()) {
        glNamedBufferData(sMeshVbo, GLsizeiptr(sVboAccum.size() * sizeof(MeshVert)),
                          sVboAccum.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(sMeshVao);

        // Pick pass (all pickable meshes)
        if (sPickEnabled) {
            BeginPickPass();
            sPickMeshShader.Use();
            sPickMeshShader.Set("uViewProj", sViewProj);
            for (auto& d : sDrawList) {
                if (!d.pickId || d.unlitMode == 3) continue;
                sPickMeshShader.Set("uPickId", d.pickId);
                glDrawArrays(GL_TRIANGLES, d.offset, d.count);
                ++sStats.pickDrawCalls;
            }
            EndPickPass();
        }

        // Glow pass (additive blending, no depth write)
        SetMeshFrameUniforms(); sMeshShader.Use();
        bool hasGlow = false;
        for (auto& d : sDrawList) {
            if (d.unlitMode != 3) continue;
            if (!hasGlow) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                hasGlow = true;
            }
            sMeshShader.Set("uColor", d.color); sMeshShader.Set("uUnlit", 3);
            glDrawArrays(GL_TRIANGLES, d.offset, d.count); ++sStats.drawCalls;
        }
        if (hasGlow) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }

        // Solid pass (lit, unlit, emissive)
        for (auto& d : sDrawList) {
            if (d.unlitMode == 3) continue;
            sMeshShader.Set("uColor", d.color); sMeshShader.Set("uUnlit", d.unlitMode);
            glDrawArrays(GL_TRIANGLES, d.offset, d.count); ++sStats.drawCalls;
        }
        sDrawList.clear(); sVboAccum.clear();
    }
    FlushPoints(); FlushLines();
    if (sFrame.scene->gridCfg.enabled) DrawGrid(sFrame.scene->gridCfg, sFrame.scene->cam.Distance());

    if (sFrame.hovered) {
        auto& io = ImGui::GetIO();
        int mx = static_cast<int>(io.MousePos.x - sFrame.cx);
        int my = static_cast<int>(sFrame.h - 1.f - (io.MousePos.y - sFrame.cy));
        sFrame.scene->hoveredPickId = sFrame.scene->pickFbo.ReadPixel(mx, my);
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
void Translate(const glm::vec3& offset) { sMatStack.back() = glm::translate(sMatStack.back(), offset); }
void Translate(float x, float y, float z) { Translate({x, y, z}); }
void Rotate(float angleDeg, const glm::vec3& axis) { sMatStack.back() = glm::rotate(sMatStack.back(), glm::radians(angleDeg), axis); }
void Rotate(const glm::quat& q) { sMatStack.back() *= glm::mat4_cast(q); }
void RotateX(float deg) { Rotate(deg, {1, 0, 0}); }
void RotateY(float deg) { Rotate(deg, {0, 1, 0}); }
void RotateZ(float deg) { Rotate(deg, {0, 0, 1}); }
void Scale(const glm::vec3& s) { sMatStack.back() = glm::scale(sMatStack.back(), s); }
void Scale(float s) { Scale({s, s, s}); }

} // namespace Kilo::Render

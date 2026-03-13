#include "Render/Draw.hpp"
#include "Render/Primitives.hpp"
#include "Render/Camera.hpp"
#include "Render/Scene.hpp"
#include <imgui.h>
#include <cassert>
#include <memory>
#include <unordered_map>

namespace Kilo::Render {

static Primitives* sCtx = nullptr;
static Camera*     sCam = nullptr;

static Primitives& Ctx() { assert(sCtx && "Render calls require active Draw3D"); return *sCtx; }

void SetContext(Primitives* prims, Camera* cam) { sCtx = prims; sCam = cam; }
Camera& GetCamera() { assert(sCam && "GetCamera requires active scene"); return *sCam; }

// ── transform stack ──────────────────────────────────────────────
void PushMatrix()                                { Ctx().PushMatrix(); }
void PopMatrix()                                 { Ctx().PopMatrix(); }
void ResetMatrix()                               { Ctx().ResetMatrix(); }
void Translate(const glm::vec3& v)               { Ctx().Translate(v); }
void Translate(float x, float y, float z)        { Ctx().Translate(x, y, z); }
void Rotate(float deg, const glm::vec3& axis)    { Ctx().Rotate(deg, axis); }
void RotateX(float deg)                          { Ctx().RotateX(deg); }
void RotateY(float deg)                          { Ctx().RotateY(deg); }
void RotateZ(float deg)                          { Ctx().RotateZ(deg); }
void Scale(const glm::vec3& s)                   { Ctx().Scale(s); }
void Scale(float s)                              { Ctx().Scale(s); }

// ── lines ────────────────────────────────────────────────────────
void Line(const glm::vec3& a, const glm::vec3& b,
          const glm::vec4& color, float width)
{ Ctx().DrawLine(a, b, color, width); }

void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg)
{ Ctx().DrawArc(center, axis, startDir, radius, angleDeg, color, seg); }

void Circle(const glm::vec3& center, const glm::vec3& axis,
            float radius, const glm::vec4& color, int seg)
{ Ctx().DrawCircle(center, axis, radius, color, seg); }

// ── basic geometry ───────────────────────────────────────────────
void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color)
{ Ctx().DrawTriangle(a, b, c, color); }

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color)
{ Ctx().DrawQuad(a, b, c, d, color); }

void Plane(const glm::vec3& center, const glm::vec3& normal,
           const glm::vec2& size, const glm::vec4& color)
{ Ctx().DrawPlane(center, normal, size, color); }

// ── mesh primitives ──────────────────────────────────────────────
void Sphere(const glm::vec3& center, float radius,
            const glm::vec4& color, int seg)
{ Ctx().DrawSphere(center, radius, color, seg); }

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color)
{ Ctx().DrawBox(center, size, color); }

void Cube(const glm::vec3& center, float size, const glm::vec4& color)
{ Ctx().DrawCube(center, size, color); }

void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg)
{ Ctx().DrawCylinder(a, b, radius, color, seg); }

void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg)
{ Ctx().DrawCone(base, tip, radius, color, seg); }

void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg)
{ Ctx().DrawCapsule(a, b, radius, color, seg); }

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg)
{ Ctx().DrawTorus(center, axis, majorR, minorR, color, seg); }

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg)
{ Ctx().DrawDisk(center, normal, radius, color, seg); }

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg)
{ Ctx().DrawRing(center, normal, innerR, outerR, color, seg); }

// ── composite ────────────────────────────────────────────────────
void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR, float headR)
{ Ctx().DrawArrow(from, to, color, shaftR, headR); }

void Axes(const glm::vec3& origin, float len)
{ Ctx().DrawAxes(origin, len); }

void Point(const glm::vec3& pos, const glm::vec4& color, float size)
{ Ctx().DrawPoint(pos, color, size); }

// ── scene viewport ──────────────────────────────────────────────

static std::string sShaderDir;
static std::unordered_map<std::string, std::unique_ptr<Scene>> sScenes;
static struct { Scene* scene{}; float cx{}, cy{}, w{}, h{}; } sFrame;

void Init(const std::string& dir) { sShaderDir = dir; }

void Begin(const char* name, const ViewportConfig& cfg) {
    auto& scene = sScenes[name];
    if (!scene) {
        scene = std::make_unique<Scene>();
        scene->Init(sShaderDir);
    }

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(cfg.width  > 0 ? cfg.width  : avail.x));
    int h = std::max(1, static_cast<int>(cfg.height > 0 ? cfg.height : avail.y));
    scene->Resize(w, h);

    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    auto& io  = ImGui::GetIO();
    auto& cam = scene->GetCamera();

    if (ImGui::IsItemHovered() && io.MouseWheel != 0)
        cam.Zoom(io.MouseWheel);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    sFrame = { scene.get(), cursor.x, cursor.y, size.x, size.y };
    scene->BeginRender();
    SetContext(&scene->Prims(), &cam);
}

void End() {
    SetContext(nullptr, &sFrame.scene->GetCamera());
    sFrame.scene->EndRender();
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(sFrame.scene->Texture())),
                 {sFrame.w, sFrame.h}, {0, 1}, {1, 0});
}

} // namespace Kilo::Render

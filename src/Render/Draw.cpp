#include "Render/Draw.hpp"
#include "Render/Primitives.hpp"
#include "Render/Camera.hpp"
#include <cassert>

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

} // namespace Kilo::Render

#pragma once
#include <glm/glm.hpp>

namespace Kilo::Render {

class Primitives;
class Camera;

// Context — set internally by Panel::Draw3D
void SetContext(Primitives* prims, Camera* cam);
Camera& GetCamera();

// ── transform stack ──────────────────────────────────────────────
void PushMatrix();
void PopMatrix();
void ResetMatrix();
void Translate(const glm::vec3& offset);
void Translate(float x, float y, float z);
void Rotate(float angleDeg, const glm::vec3& axis);
void RotateX(float angleDeg);
void RotateY(float angleDeg);
void RotateZ(float angleDeg);
void Scale(const glm::vec3& s);
void Scale(float s);

// ── lines ────────────────────────────────────────────────────────
void Line(const glm::vec3& a, const glm::vec3& b,
          const glm::vec4& color, float width = 2.5f);
void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg = 32);
void Circle(const glm::vec3& center, const glm::vec3& axis,
            float radius, const glm::vec4& color, int seg = 32);

// ── basic geometry ───────────────────────────────────────────────
void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color);
void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color);
void Plane(const glm::vec3& center, const glm::vec3& normal,
           const glm::vec2& size, const glm::vec4& color);

// ── mesh primitives ──────────────────────────────────────────────
void Sphere(const glm::vec3& center, float radius,
            const glm::vec4& color, int seg = 32);
void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color);
void Cube(const glm::vec3& center, float size, const glm::vec4& color);
void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg = 16);
void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg = 24);
void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg = 16);
void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg = 32);
void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg = 32);
void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg = 32);

// ── composite ────────────────────────────────────────────────────
void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR = 0.02f, float headR = 0.06f);
void Axes(const glm::vec3& origin, float len = 1.f);
void Point(const glm::vec3& pos, const glm::vec4& color, float size = 0.05f);

} // namespace Kilo::Render

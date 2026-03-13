#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Kilo::Render {

class Camera;

// ── scene viewport ──────────────────────────────────────────────
struct ViewportConfig {
    float width  = -1;  // -1 = fill available
    float height = -1;
};

void Init(const std::string& shaderDir);
void Begin(const char* name, const ViewportConfig& cfg = {});
void End();

Camera& GetCamera();
Camera& GetCamera(const char* name);

struct Environment {
    glm::vec3 lightDir = {.5f, .3f, 1.f};  // normalized when used
    glm::vec3 bgColor  = {.12f, .12f, .14f};
    float ambient    = 0.22f;
    float diffuse    = 0.7f;
    float roughness  = 0.35f;
    float specular   = 0.15f;
    float fresnel    = 0.25f;
    float fogDensity = 0.00015f;
    bool  showSun    = false;
    float sunDistance = 20.f;
    float sunRadius  = 0.4f;
};

Environment& GetEnvironment();
Environment& GetEnvironment(const char* name);

// ── grid ────────────────────────────────────────────────────────
struct GridConfig {
    bool      enabled     = false;
    float     scaleFine   = 1.f;
    float     scaleMedium = 10.f;
    float     scaleCoarse = 100.f;
    glm::vec3 colorFine   = {.30f, .32f, .38f};
    glm::vec3 colorMedium = {.36f, .38f, .44f};
    glm::vec3 colorCoarse = {.48f, .52f, .58f};
    float     alphaFine   = 0.35f;
    float     alphaMedium = 0.50f;
    float     alphaCoarse = 0.65f;
    glm::vec3 axisXColor  = {.8f, .2f, .2f};
    glm::vec3 axisYColor  = {.2f, .8f, .2f};
    float     axisThickness = 0.006f;
    float     axisAlpha   = 0.75f;
    float     fadeStart   = 2.5f;
    float     fadeEnd     = 10.f;
};

void Grid();
void Grid(const GridConfig& cfg);
GridConfig& GetGrid();
GridConfig& GetGrid(const char* name);

// ── transform stack ──────────────────────────────────────────────
void PushMatrix();
void PopMatrix();
void ResetMatrix();
void SetMatrix(const glm::mat4& m);
void Transform(const glm::mat4& m);
void Translate(const glm::vec3& offset);
void Translate(float x, float y, float z);
void Rotate(float angleDeg, const glm::vec3& axis);
void Rotate(const glm::quat& q);
void RotateX(float angleDeg);
void RotateY(float angleDeg);
void RotateZ(float angleDeg);
void Scale(const glm::vec3& s);
void Scale(float s);

// ── lines ────────────────────────────────────────────────────────
void Line(const glm::vec3& a, const glm::vec3& b,
          const glm::vec4& color, float width = 2.5f);
void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width = 2.5f, bool closed = false);
void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg = 32, float width = 2.5f);
void Circle(const glm::vec3& center, const glm::vec3& axis,
            float radius, const glm::vec4& color, int seg = 32, float width = 2.5f);

// ── basic geometry ───────────────────────────────────────────────
void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color);
void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color);
void Plane(const glm::vec3& center, const glm::vec3& normal,
           const glm::vec2& halfSize, const glm::vec4& color);

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

// ── wireframe ────────────────────────────────────────────────────
void WireBox(const glm::vec3& center, const glm::vec3& size,
             const glm::vec4& color, float width = 2.5f);
void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg = 32, float width = 2.5f);

// ── composite ────────────────────────────────────────────────────
void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR = 0.02f, float headR = 0.06f);
void Axes(const glm::vec3& origin, float len = 1.f);
void Point(const glm::vec3& pos, const glm::vec4& color, float size = 0.05f);
void Cross(const glm::vec3& pos, float size,
           const glm::vec4& color, float width = 2.5f);
void AABB(const glm::vec3& min, const glm::vec3& max,
          const glm::vec4& color, float width = 2.5f);
void WireGrid(const glm::vec3& center, const glm::vec3& normal,
              float size, int divisions, const glm::vec4& color, float width = 1.f);
void Frustum(const glm::mat4& viewProj,
             const glm::vec4& color, float width = 2.5f);

} // namespace Kilo::Render

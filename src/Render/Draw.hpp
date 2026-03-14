#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <cmath>

namespace Kilo::Render {

class Camera;

// ── colors ──────────────────────────────────────────────────────
namespace Color {

inline constexpr glm::vec4 Red     {.95f, .25f, .25f, 1.f};
inline constexpr glm::vec4 Green   {.35f, .85f, .35f, 1.f};
inline constexpr glm::vec4 Blue    {.35f, .50f, .95f, 1.f};
inline constexpr glm::vec4 Yellow  {1.f,  .92f, .30f, 1.f};
inline constexpr glm::vec4 Cyan    {.30f, .90f, .95f, 1.f};
inline constexpr glm::vec4 Magenta {.90f, .35f, .90f, 1.f};
inline constexpr glm::vec4 Orange  {1.f,  .60f, .20f, 1.f};
inline constexpr glm::vec4 White   {1.f,  1.f,  1.f,  1.f};
inline constexpr glm::vec4 Gray    {.55f, .55f, .55f, 1.f};
inline constexpr glm::vec4 Black   {.05f, .05f, .05f, 1.f};

inline constexpr glm::vec4 WithAlpha(glm::vec4 c, float a) {
    return {c.r, c.g, c.b, a};
}

inline glm::vec4 Hue(float t) {
    float h = t - std::floor(t);
    float s = h * 6.f;
    int   i = static_cast<int>(s);
    float f = s - static_cast<float>(i);
    float q = 1.f - f, v = f;
    switch (i % 6) {
        case 0: return {1.f, v,   0.f, 1.f};
        case 1: return {q,   1.f, 0.f, 1.f};
        case 2: return {0.f, 1.f, v,   1.f};
        case 3: return {0.f, q,   1.f, 1.f};
        case 4: return {v,   0.f, 1.f, 1.f};
        default:return {1.f, 0.f, q,   1.f};
    }
}

} // namespace Color

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
    glm::vec3 lightDir = {.5f, .3f, 1.f};
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
    float     axisAlpha   = 1.f;
    bool      axisScaleWithCam = true;
    float     fadeStart   = 2.5f;
    float     fadeEnd     = 10.f;
};

void Grid();
void Grid(const GridConfig& cfg);
GridConfig& GetGrid();
GridConfig& GetGrid(const char* name);

// ── projection helpers ──────────────────────────────────────────
glm::vec2 WorldToScreen(const glm::vec3& worldPos);

// ── interaction ─────────────────────────────────────────────────
struct EventState {
    bool hovered_ = false;
    bool Hovered() const { return hovered_; }
    bool Clicked(int button = 0) const;
};

EventState Event();

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
void Path(const glm::vec3* points, const glm::vec4* colors,
          int count, float width = 2.5f, bool closed = false);
void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg = 32, float width = 2.5f);
void Circle(const glm::vec3& center, const glm::vec3& axis,
            float radius, const glm::vec4& color, int seg = 32, float width = 2.5f);
void Spline(const glm::vec3* controlPoints, int count,
            const glm::vec4& color, int segments = 32, float width = 2.5f);

// ── points ───────────────────────────────────────────────────────
void Points(const glm::vec3* positions, int count,
            const glm::vec4& color, float size = 4.f);
void Points(const glm::vec3* positions, const glm::vec4* colors,
            int count, float size = 4.f);

// ── text ─────────────────────────────────────────────────────────
void Text(const glm::vec3& pos, const glm::vec4& color,
          const char* fmt, ...) __attribute__((format(printf, 3, 4)));

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

// ── custom mesh ──────────────────────────────────────────────────
void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          const uint32_t* indices, int indexCount, const glm::vec4& color);
void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          int vertCount, const glm::vec4& color);

// ── wireframe ────────────────────────────────────────────────────
void WireBox(const glm::vec3& center, const glm::vec3& size,
             const glm::vec4& color, float width = 2.5f);
void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg = 32, float width = 2.5f);
void WireCylinder(const glm::vec3& a, const glm::vec3& b,
                  float radius, const glm::vec4& color, int seg = 16, float width = 2.5f);
void WireCone(const glm::vec3& base, const glm::vec3& tip,
              float radius, const glm::vec4& color, int seg = 16, float width = 2.5f);
void WireCapsule(const glm::vec3& a, const glm::vec3& b,
                 float radius, const glm::vec4& color, int seg = 16, float width = 2.5f);

// ── composite ────────────────────────────────────────────────────
void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR = 0.02f, float headR = 0.06f);
void Axes(const glm::vec3& origin, float len = 1.f);
void Frame(const glm::mat4& pose, float len = 0.3f);
void Frame(const glm::vec3& pos, const glm::quat& orient, float len = 0.3f);
void Point(const glm::vec3& pos, const glm::vec4& color, float size = 0.05f);
void Cross(const glm::vec3& pos, float size,
           const glm::vec4& color, float width = 2.5f);
void AABB(const glm::vec3& min, const glm::vec3& max,
          const glm::vec4& color, float width = 2.5f);
void OBB(const glm::vec3& center, const glm::quat& orient,
         const glm::vec3& size, const glm::vec4& color, float width = 2.5f);
void Covariance(const glm::vec3& pos, const glm::mat3& cov,
                const glm::vec4& color, float sigma = 2.f, int seg = 24);
void WireGrid(const glm::vec3& center, const glm::vec3& normal,
              float size, int divisions, const glm::vec4& color, float width = 1.f);
void Frustum(const glm::mat4& viewProj,
             const glm::vec4& color, float width = 2.5f);

} // namespace Kilo::Render

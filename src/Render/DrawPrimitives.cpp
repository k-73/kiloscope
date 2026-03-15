#include "Render/DrawState.hpp"
#include <generator/SphereMesh.hpp>
#include <generator/BoxMesh.hpp>
#include <generator/CappedCylinderMesh.hpp>
#include <generator/CappedConeMesh.hpp>
#include <generator/CapsuleMesh.hpp>
#include <generator/TorusMesh.hpp>
#include <generator/DiskMesh.hpp>

namespace Kilo::Render {

// ── lines ────────────────────────────────────────────────────────────

void Line(const glm::vec3& a, const glm::vec3& b,
          const glm::vec4& color, float width) {
    ctx().lastPickId = AllocPickId();
    BatchLine(XformPoint(a), XformPoint(b), color, width);
}

void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width, bool closed) {
    if (count < 2) return;
    ctx().lastPickId = AllocPickId();
    auto first = XformPoint(points[0]);
    auto prev = first;
    for (int i = 1; i < count; ++i) {
        auto cur = XformPoint(points[i]);
        BatchLine(prev, cur, color, width);
        prev = cur;
    }
    if (closed && count > 2) BatchLine(prev, first, color, width);
}

void Path(const glm::vec3* points, const glm::vec4* colors,
          int count, float width, bool closed) {
    if (count < 2) return;
    ctx().lastPickId = AllocPickId();
    auto first = XformPoint(points[0]);
    auto prev = first;
    for (int i = 1; i < count; ++i) {
        auto cur = XformPoint(points[i]);
        BatchLineGradient(prev, cur, colors[i - 1], colors[i], width);
        prev = cur;
    }
    if (closed && count > 2) BatchLineGradient(prev, first, colors[count - 1], colors[0], width);
}

void Arc(const glm::vec3& center, const glm::vec3& axis,
         const glm::vec3& startDir, float radius,
         float angleDeg, const glm::vec4& color, int seg, float width) {
    ctx().lastPickId = AllocPickId();
    auto tc = XformPoint(center);
    auto ta = glm::normalize(XformDir(axis));
    auto ts = glm::normalize(XformDir(startDir));
    float step = glm::radians(angleDeg) / static_cast<float>(seg);
    auto prev = tc + ts * radius;
    for (int i = 1; i <= seg; ++i) {
        auto cur = tc + glm::vec3(glm::rotate(glm::mat4(1), step * i, ta)
                                  * glm::vec4(ts * radius, 0));
        BatchLine(prev, cur, color, width);
        prev = cur;
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

    ctx().lastPickId = AllocPickId();
    // Catmull-Rom: C1-continuous interpolation through control points
    auto catmullRom = [](const glm::vec3& p0, const glm::vec3& p1,
                         const glm::vec3& p2, const glm::vec3& p3, float t) {
        return 0.5f * ((2.f * p1) +
            (-p0 + p2) * t +
            (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
            (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
    };

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
    auto& s = ctx();
    s.lastPickId = AllocPickId();
    if (!s.pointBatch.empty() && size != s.pointSize) FlushPoints();
    s.pointSize = size;
    uint32_t pid = s.lastPickId;
    for (int i = 0; i < count; ++i)
        s.pointBatch.push_back({XformPoint(positions[i]), color, pid});
}

void Points(const glm::vec3* positions, const glm::vec4* colors,
            int count, float size) {
    auto& s = ctx();
    s.lastPickId = AllocPickId();
    if (!s.pointBatch.empty() && size != s.pointSize) FlushPoints();
    s.pointSize = size;
    uint32_t pid = s.lastPickId;
    for (int i = 0; i < count; ++i)
        s.pointBatch.push_back({XformPoint(positions[i]), colors[i], pid});
}

// ── text ─────────────────────────────────────────────────────────────

void Text(const glm::vec3& pos, const glm::vec4& color, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ctx().textBatch.push_back({XformPoint(pos), color, buf});
    ++ctx().stats.textLabels;
}

// ── basic geometry ───────────────────────────────────────────────────

void Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
              const glm::vec4& color) {
    ctx().lastPickId = AllocPickId();
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto normal = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    UploadMesh({{ta, normal}, {tb, normal}, {tc, normal}});
}

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    ctx().lastPickId = AllocPickId();
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
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendFromCache(sMeshScratch, GetUnitSphere(seg),
                    glm::scale(glm::translate(Mat(), center), glm::vec3(radius)));
    UploadMesh(sMeshScratch);
}

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color) {
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    auto xform = glm::translate(Mat(), center) * glm::scale(glm::mat4(1.f), size);
    AppendFromCache(sMeshScratch, GetUnitBox(), xform);
    UploadMesh(sMeshScratch);
}

void Cube(const glm::vec3& center, float size, const glm::vec4& color) {
    Box(center, glm::vec3(size), color);
}

void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; } // degenerate → sphere
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CappedCylinderMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(a, b));
    UploadMesh(sMeshScratch);
}

void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(tip - base) * 0.5f;
    if (halfLen < 1e-6f) { Sphere(base, radius, color, seg); return; } // degenerate → sphere
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CappedConeMesh(radius, halfLen, seg, 1, 1),
               Mat() * ZAlign(base, tip));
    UploadMesh(sMeshScratch);
}

void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; } // degenerate → sphere
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CapsuleMesh(radius, halfLen, seg, 1, seg / 2),
               Mat() * ZAlign(a, b));
    UploadMesh(sMeshScratch);
}

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg) {
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(sMeshScratch);
}

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg) {
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::DiskMesh(radius, 0.0, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(sMeshScratch);
}

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg) {
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(sMeshScratch);
}

// ── custom mesh ──────────────────────────────────────────────────────

static void MeshImpl(const glm::vec3* verts, const glm::vec3* normals,
                     int count, const uint32_t* indices, const glm::vec4& color) {
    ctx().lastPickId = AllocPickId();
    SetMeshUniforms(color);
    auto& m = Mat();
    auto nmat = glm::transpose(glm::inverse(glm::mat3(m)));
    sMeshScratch.clear();
    sMeshScratch.reserve(count);
    for (int i = 0; i < count; ++i) {
        int idx = indices ? static_cast<int>(indices[i]) : i;
        MeshVert v;
        v.pos    = glm::vec3(m * glm::vec4(verts[idx], 1.f));
        v.normal = glm::normalize(nmat * normals[idx]);
        sMeshScratch.push_back(v);
    }
    UploadMesh(sMeshScratch);
}

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          const uint32_t* indices, int indexCount, const glm::vec4& color) {
    MeshImpl(verts, normals, indexCount, indices, color);
}

void Mesh(const glm::vec3* verts, const glm::vec3* normals,
          int vertCount, const glm::vec4& color) {
    MeshImpl(verts, normals, vertCount, nullptr, color);
}

// ── wireframe ────────────────────────────────────────────────────────

void WireBox(const glm::vec3& center, const glm::vec3& size,
             const glm::vec4& color, float width) {
    PickGroup pg;
    auto hs = size * 0.5f;
    glm::vec3 c[8] = {
        center + glm::vec3(-hs.x, -hs.y, -hs.z), center + glm::vec3( hs.x, -hs.y, -hs.z),
        center + glm::vec3( hs.x,  hs.y, -hs.z), center + glm::vec3(-hs.x,  hs.y, -hs.z),
        center + glm::vec3(-hs.x, -hs.y,  hs.z), center + glm::vec3( hs.x, -hs.y,  hs.z),
        center + glm::vec3( hs.x,  hs.y,  hs.z), center + glm::vec3(-hs.x,  hs.y,  hs.z),
    };
    for (int i = 0; i < 4; ++i) {
        Line(c[i], c[(i+1)%4], color, width);
        Line(c[i+4], c[(i+1)%4+4], color, width);
        Line(c[i], c[i+4], color, width);
    }
}

void WireSphere(const glm::vec3& center, float radius,
                const glm::vec4& color, int seg, float width) {
    PickGroup pg;
    Circle(center, {1, 0, 0}, radius, color, seg, width);
    Circle(center, {0, 1, 0}, radius, color, seg, width);
    Circle(center, {0, 0, 1}, radius, color, seg, width);
}

void WireCylinder(const glm::vec3& a, const glm::vec3& b,
                  float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    PickGroup pg;
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
}

void WireCone(const glm::vec3& base, const glm::vec3& tip,
              float radius, const glm::vec4& color, int seg, float width) {
    auto axis = tip - base;
    float len = glm::length(axis);
    if (len < 1e-6f) return;
    PickGroup pg;
    auto dir  = axis / len;
    auto perp = Perpendicular(dir);
    auto side = glm::cross(dir, perp);
    Circle(base, dir, radius, color, seg, width);
    for (int i = 0; i < 4; ++i) {
        float angle = glm::half_pi<float>() * static_cast<float>(i);
        auto d = perp * std::cos(angle) + side * std::sin(angle);
        Line(base + d * radius, tip, color, width);
    }
}

void WireCapsule(const glm::vec3& a, const glm::vec3& b,
                 float radius, const glm::vec4& color, int seg, float width) {
    auto axis = b - a;
    float len = glm::length(axis);
    if (len < 1e-6f) { WireSphere((a + b) * 0.5f, radius, color, seg, width); return; }
    PickGroup pg;
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
}

// ── composite ────────────────────────────────────────────────────────

void Arrow(const glm::vec3& from, const glm::vec3& to,
           const glm::vec4& color, float shaftR, float headR) {
    auto dir = to - from;
    float len = glm::length(dir);
    if (len < 1e-6f) return;
    ctx().lastPickId = AllocPickId();
    float headLen = std::min(len * 0.25f, headR * 2.5f);
    auto shaftEnd = from + dir * ((len - headLen) / len);
    SetMeshUniforms(color);
    sMeshScratch.clear();
    AppendMesh(sMeshScratch, generator::CappedCylinderMesh(shaftR, glm::length(shaftEnd - from) * 0.5f, 24, 1, 1),
               Mat() * ZAlign(from, shaftEnd));
    AppendMesh(sMeshScratch, generator::CappedConeMesh(headR, headLen * 0.5f, 24, 1, 1),
               Mat() * ZAlign(shaftEnd, to));
    UploadMesh(sMeshScratch);
}

void Axes(const glm::vec3& origin, float len) {
    PickGroup pg;
    float s = len * 0.025f, h = len * 0.07f;
    Arrow(origin, origin + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
}

void Frame(const glm::mat4& pose, float len) {
    PickGroup pg;
    PushMatrix(); Transform(pose); Axes({0, 0, 0}, len); PopMatrix();
}

void Frame(const glm::vec3& pos, const glm::quat& orient, float len) {
    Frame(glm::translate(glm::mat4(1.f), pos) * glm::mat4_cast(orient), len);
}

void Point(const glm::vec3& pos, const glm::vec4& color, float size) {
    Sphere(pos, size, color, 8);
}

void SphereLight(const glm::vec3& pos, float radius,
                 const glm::vec4& color, float range) {
    PointLight(pos, glm::vec3(color), range); // illuminate scene
    SetNextEmissive(radius * kGlowRadiusScale);
    Sphere(pos, radius, color);
}

void BoxLight(const glm::vec3& center, const glm::vec3& size,
              const glm::vec4& color, float range) {
    PointLight(center, glm::vec3(color), range);
    SetNextEmissive(glm::length(size));
    Box(center, size, color);
}

void Cross(const glm::vec3& pos, float size, const glm::vec4& color, float width) {
    PickGroup pg;
    float hs = size * 0.5f;
    Line(pos - glm::vec3(hs, 0, 0), pos + glm::vec3(hs, 0, 0), color, width);
    Line(pos - glm::vec3(0, hs, 0), pos + glm::vec3(0, hs, 0), color, width);
    Line(pos - glm::vec3(0, 0, hs), pos + glm::vec3(0, 0, hs), color, width);
}

void AABB(const glm::vec3& mn, const glm::vec3& mx,
          const glm::vec4& color, float width) {
    PickGroup pg; WireBox((mn + mx) * 0.5f, mx - mn, color, width);
}

void OBB(const glm::vec3& center, const glm::quat& orient,
         const glm::vec3& size, const glm::vec4& color, float width) {
    PickGroup pg;
    PushMatrix(); Translate(center); Rotate(orient);
    WireBox({0, 0, 0}, size, color, width);
    PopMatrix();
}

void Covariance(const glm::vec3& pos, const glm::mat3& cov,
                const glm::vec4& color, float sigma, int seg) {
    glm::vec3 eigvals; glm::mat3 eigvecs;
    Eigen3(cov, eigvals, eigvecs);
    glm::vec3 radii = sigma * glm::vec3(
        std::sqrt(std::max(eigvals.x, 0.f)),
        std::sqrt(std::max(eigvals.y, 0.f)),
        std::sqrt(std::max(eigvals.z, 0.f)));
    PushMatrix(); Translate(pos);
    Transform(glm::mat4(glm::vec4(eigvecs[0], 0), glm::vec4(eigvecs[1], 0),
                         glm::vec4(eigvecs[2], 0), glm::vec4(0, 0, 0, 1)));
    Scale(radii); Sphere({0, 0, 0}, 1.f, color, seg); PopMatrix();
}

void WireGrid(const glm::vec3& center, const glm::vec3& normal,
              float size, int divisions, const glm::vec4& color, float width) {
    PickGroup pg;
    auto n = glm::normalize(normal);
    auto u = Perpendicular(n);
    auto v = glm::cross(n, u);
    float half = size * 0.5f, step = size / static_cast<float>(divisions);
    for (int i = 0; i <= divisions; ++i) {
        float t = -half + step * static_cast<float>(i);
        Line(center + u * t - v * half, center + u * t + v * half, color, width);
        Line(center - u * half + v * t, center + u * half + v * t, color, width);
    }
}

void Frustum(const glm::mat4& viewProj, const glm::vec4& color, float width) {
    PickGroup pg;
    glm::mat4 inv = glm::inverse(viewProj);
    auto unproject = [&](float x, float y, float z) -> glm::vec3 {
        glm::vec4 p = inv * glm::vec4(x, y, z, 1.f);
        return glm::vec3(p) / p.w;
    };
    glm::vec3 n[4] = { unproject(-1,-1,-1), unproject(1,-1,-1),
                        unproject(1,1,-1), unproject(-1,1,-1) };
    glm::vec3 f[4] = { unproject(-1,-1,1), unproject(1,-1,1),
                        unproject(1,1,1), unproject(-1,1,1) };
    for (int i = 0; i < 4; ++i) {
        Line(n[i], n[(i+1)%4], color, width);
        Line(f[i], f[(i+1)%4], color, width);
        Line(n[i], f[i], color, width);
    }
}

} // namespace Kilo::Render

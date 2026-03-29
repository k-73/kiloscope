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
    ctx().activePickId = AllocPickId();
    BatchLine(XformPoint(a), XformPoint(b), color, width);
}

void Polyline(const glm::vec3* points, int count,
              const glm::vec4& color, float width, bool closed) {
    if (count < 2) return;
    ctx().activePickId = AllocPickId();
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
    ctx().activePickId = AllocPickId();
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
    ctx().activePickId = AllocPickId();
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

    ctx().activePickId = AllocPickId();
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

void Trail(const glm::vec3* points, int count,
           const glm::vec4& color, float width) {
    if (count < 2) return;
    ctx().activePickId = AllocPickId();
    float inv = 1.f / static_cast<float>(count - 1);
    for (int i = 0; i + 1 < count; ++i) {
        float a0 = color.a * static_cast<float>(i) * inv;
        float a1 = color.a * static_cast<float>(i + 1) * inv;
        BatchLineGradient(XformPoint(points[i]), XformPoint(points[i + 1]),
                          {color.r, color.g, color.b, a0},
                          {color.r, color.g, color.b, a1}, width);
    }
}

// ── points ───────────────────────────────────────────────────────────

void Points(const glm::vec3* positions, int count,
            const glm::vec4& color, float size) {
    auto& s = ctx();
    s.activePickId = AllocPickId();
    if (!s.pointBatch.empty() && size != s.pointSize) FlushPoints();
    s.pointSize = size;
    uint32_t pid = s.activePickId;
    for (int i = 0; i < count; ++i)
        s.pointBatch.push_back({XformPoint(positions[i]), color, pid});
}

void Points(const glm::vec3* positions, const glm::vec4* colors,
            int count, float size) {
    auto& s = ctx();
    s.activePickId = AllocPickId();
    if (!s.pointBatch.empty() && size != s.pointSize) FlushPoints();
    s.pointSize = size;
    uint32_t pid = s.activePickId;
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
              const glm::vec4& color, bool twoSided) {
    ctx().activePickId = AllocPickId();
    auto ta = XformPoint(a), tb = XformPoint(b), tc = XformPoint(c);
    auto n = glm::normalize(glm::cross(tb - ta, tc - ta));
    SetMeshUniforms(color);
    if (twoSided)
        UploadMesh({{ta, n}, {tb, n}, {tc, n}, {ta, -n}, {tc, -n}, {tb, -n}});
    else
        UploadMesh({{ta, n}, {tb, n}, {tc, n}});
}

void Quad(const glm::vec3& a, const glm::vec3& b,
          const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    ctx().activePickId = AllocPickId();
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
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitSphere(seg),
                  glm::scale(glm::translate(Mat(), center), glm::vec3(radius)));
}

void Box(const glm::vec3& center, const glm::vec3& size,
         const glm::vec4& color) {
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitBox(),
                  glm::translate(Mat(), center) * glm::scale(glm::mat4(1.f), size));
}

void Cube(const glm::vec3& center, float size, const glm::vec4& color) {
    Box(center, glm::vec3(size), color);
}

void Cylinder(const glm::vec3& a, const glm::vec3& b,
              float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; }
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitCylinder(seg),
                  Mat() * ZAlign(a, b) * glm::scale(glm::mat4(1.f), {radius, radius, halfLen}));
}

void Cone(const glm::vec3& base, const glm::vec3& tip,
          float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(tip - base) * 0.5f;
    if (halfLen < 1e-6f) { Sphere(base, radius, color, seg); return; }
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitCone(seg),
                  Mat() * ZAlign(base, tip) * glm::scale(glm::mat4(1.f), {radius, radius, halfLen}));
}

void Capsule(const glm::vec3& a, const glm::vec3& b,
             float radius, const glm::vec4& color, int seg) {
    float halfLen = glm::length(b - a) * 0.5f;
    if (halfLen < 1e-6f) { Sphere((a + b) * 0.5f, radius, color, seg); return; }
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitCapsule(seg),
                  Mat() * ZAlign(a, b) * glm::scale(glm::mat4(1.f), {radius, radius, halfLen}));
}

void Torus(const glm::vec3& center, const glm::vec3& axis,
           float majorR, float minorR, const glm::vec4& color, int seg) {
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    // Generate directly — non-uniform scale on a unit torus distorts the tube cross-section.
    AppendMesh(sMeshScratch, generator::TorusMesh(minorR, majorR, seg / 2, seg),
               Mat() * AxisTransform(center, axis));
    UploadMesh(sMeshScratch);
}

void Disk(const glm::vec3& center, const glm::vec3& normal,
          float radius, const glm::vec4& color, int seg) {
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitDisk(seg),
                  Mat() * AxisTransform(center, normal) * glm::scale(glm::mat4(1.f), glm::vec3(radius)));
}

void Ring(const glm::vec3& center, const glm::vec3& normal,
          float innerR, float outerR, const glm::vec4& color, int seg) {
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    sMeshScratch.clear();
    // Ring can't use unit cache (inner/outer ratio varies), generate directly
    AppendMesh(sMeshScratch, generator::DiskMesh(outerR, innerR, seg, 1),
               Mat() * AxisTransform(center, normal));
    UploadMesh(sMeshScratch);
}

// ── custom mesh ──────────────────────────────────────────────────────

static void MeshImpl(const glm::vec3* verts, const glm::vec3* normals,
                     int count, const uint32_t* indices, const glm::vec4& color) {
    ctx().activePickId = AllocPickId();
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
    ctx().activePickId = AllocPickId();
    float headLen = std::min(len * 0.25f, headR * 2.5f);
    float shaftHalf = (len - headLen) * 0.5f;
    auto shaftEnd = from + dir * ((len - headLen) / len);
    SetMeshUniforms(color);
    UploadGpuDraw(GetUnitCylinder(24),
                  Mat() * ZAlign(from, shaftEnd) * glm::scale(glm::mat4(1.f), {shaftR, shaftR, shaftHalf}));
    UploadGpuDraw(GetUnitCone(24),
                  Mat() * ZAlign(shaftEnd, to) * glm::scale(glm::mat4(1.f), {headR, headR, headLen * 0.5f}));
}

void Axes(const glm::vec3& origin, float len) {
    PickGroup pg;
    float s = len * 0.025f, h = len * 0.07f;
    Arrow(origin, origin + glm::vec3(len, 0, 0), {.95f, .25f, .25f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, len, 0), {.35f, .85f, .35f, 1}, s, h);
    Arrow(origin, origin + glm::vec3(0, 0, len), {.35f, .50f, .95f, 1}, s, h);
}

void Pose(const glm::vec3& pos, float len) {
    PickGroup pg;
    Axes(pos, len);
}

void Pose(const glm::mat4& pose, float len) {
    PickGroup pg;
    PushMatrix(); Transform(pose); Axes({0, 0, 0}, len); PopMatrix();
}

void Pose(const glm::vec3& pos, const glm::quat& orient, float len) {
    Pose(glm::translate(glm::mat4(1.f), pos) * glm::mat4_cast(orient), len);
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

bool Marker(const glm::vec3& pos, const char* icon, const char* label,
            const glm::vec4& color, const char* detailFmt, ...) {
    auto p = XformPoint(pos);
    auto scr = WorldToScreen(p);
    if (scr.x < 0.f) return false;

    constexpr float kGap = 3.f, kLabelUp = 2.f;
    auto iconSz = ImGui::CalcTextSize(icon);
    auto lblSz  = ImGui::CalcTextSize(label);
    glm::vec4 lblCol = {color.r, color.g, color.b, color.a * .8f};

    // Icon (centered on point) + label (right of icon, aligned to icon center)
    ctx().textBatch.push_back({p, color, icon, true});
    ctx().textBatch.push_back({p, lblCol, label, false,
        {scr.x + iconSz.x * .5f + kGap, scr.y - lblSz.y * .5f - kLabelUp}});
    ctx().stats.textLabels += 2;

    // Hover: bounding box of icon + label
    float hx = scr.x - iconSz.x * .5f;
    float hy = scr.y - std::max(iconSz.y, lblSz.y) * .5f - kLabelUp;
    bool hovered = ImGui::IsMouseHoveringRect(
        {hx, hy}, {hx + iconSz.x + kGap + lblSz.x, hy + std::max(iconSz.y, lblSz.y) + kLabelUp}, false);

    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::TextColored({color.r, color.g, color.b, 1.f}, "%s %s", icon, label);
        ImGui::Separator();
        auto fp = glm::transpose(ctx().frameMat) * p;
        const char* fn = "XYZ";
        if (ctx().frameMat == NED::M) fn = "NED";
        else if (ctx().frameMat == ENU::M) fn = "ENU";
        else if (ctx().frameMat == FLU::M) fn = "FLU";
        else if (ctx().frameMat == FRD::M) fn = "FRD";
        ImGui::Text("%s  %.1f  %.1f  %.1f", fn, fp.x, fp.y, fp.z);
        ImGui::Text("Cam  %.0f m", glm::length(p - sCamPos));
        if (detailFmt) {
            char buf[256];
            va_list args; va_start(args, detailFmt);
            vsnprintf(buf, sizeof buf, detailFmt, args);
            va_end(args);
            ImGui::Separator();
            ImGui::TextUnformatted(buf);
        }
        ImGui::EndTooltip();
    }

    return hovered && ImGui::IsMouseClicked(0);
}

bool HudBegin() {
    ImGui::SetCursorScreenPos({sFrame.cx, sFrame.cy});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0, 0, 0, 0});
    return ImGui::BeginChild("##hud", {sFrame.w, sFrame.h}, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void HudEnd() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Crosshair(float gap, float len, const glm::vec4& color) {
    auto p  = ImGui::GetWindowContentRegionMin();
    auto sz = ImGui::GetWindowContentRegionMax();
    auto wp = ImGui::GetWindowPos();
    auto* dl = ImGui::GetWindowDrawList();
    float cx = wp.x + (p.x + sz.x) * 0.5f, cy = wp.y + (p.y + sz.y) * 0.5f;
    ImU32 c = ImGui::ColorConvertFloat4ToU32({color.r, color.g, color.b, color.a});
    dl->AddLine({cx - gap - len, cy}, {cx - gap, cy}, c);
    dl->AddLine({cx + gap, cy}, {cx + gap + len, cy}, c);
    dl->AddLine({cx, cy - gap - len}, {cx, cy - gap}, c);
    dl->AddLine({cx, cy + gap}, {cx, cy + gap + len}, c);
}

void HUD() {
    if (!sFrame.scene) return;
    auto& sc  = *sFrame.scene;
    auto& cam = sc.cam;
    auto  pos = cam.Position();

    struct FI { glm::mat3 m; const char* name; bool geo; };
    static const FI kF[] = {
        {NED::M,"NED",true}, {ENU::M,"ENU",true},
        {XYZ::M,"XYZ",false}, {FLU::M,"FLU",false}, {FRD::M,"FRD",false},
    };
    const char* fn = "?"; bool geo = false;
    for (auto& f : kF) if (sc.frameMat == f.m) { fn = f.name; geo = f.geo; break; }

    auto wp = ImGui::GetWindowPos();
    auto lo = ImGui::GetWindowContentRegionMin(), hi = ImGui::GetWindowContentRegionMax();
    float h = ImGui::GetTextLineHeight() + 6.f;
    float x0 = wp.x + lo.x, x1 = wp.x + hi.x, y = wp.y + hi.y - h;
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({x0, y}, {x1, y + h}, IM_COL32(10, 12, 18, 210));

    ImU32 lbl = IM_COL32(65, 70, 85, 255);
    ImU32 val = IM_COL32(160, 170, 185, 255);
    ImU32 sep = IM_COL32(40, 45, 60, 255);
    float x = x0 + 6.f, ty = y + 3.f;
    char buf[96];

    auto put = [&](const char* s, ImU32 c, const char* w = nullptr) {
        dl->AddText({x, ty}, c, s); x += ImGui::CalcTextSize(w ? w : s).x;
    };
    auto div = [&] { x += 5; dl->AddLine({x, y + 3}, {x, y + h - 3}, sep); x += 5; };

    // Position
    snprintf(buf, sizeof buf, "%6.1f %6.1f %6.1f", pos.x, pos.y, pos.z);
    put(buf, val, "-000.0 -000.0 -000.0"); div();

    // Heading
    auto vd = cam.ViewDir();
    float hdg = glm::degrees(std::atan2(vd.x, vd.y));
    snprintf(buf, sizeof buf, "%4.0f\xc2\xb0", hdg);
    put(geo ? "H" : "Y", lbl); put(buf, val, "-000\xc2\xb0");

    // Compass (geo only) — ticks, no text
    if (geo) {
        float r = h * .22f, cx = x + r + 3, cy = y + h * .5f;
        float na = glm::radians(-hdg + 90.f);
        dl->AddCircle({cx, cy}, r, IM_COL32(45, 50, 65, 180), 16);
        dl->AddLine({cx + std::cos(na) * r * .45f, cy - std::sin(na) * r * .45f},
                    {cx + std::cos(na) * r,         cy - std::sin(na) * r},
                    IM_COL32(200, 65, 65, 255), 2.f);
        float sa = na + glm::pi<float>();
        dl->AddLine({cx + std::cos(sa) * r * .55f, cy - std::sin(sa) * r * .55f},
                    {cx + std::cos(sa) * r,         cy - std::sin(sa) * r},
                    IM_COL32(70, 70, 80, 150), 1.f);
        x = cx + r + 1;
    }
    div();

    // Pitch, FOV, Distance
    snprintf(buf, sizeof buf, "%3.0f\xc2\xb0", cam.Pitch());
    put("P", lbl); put(buf, val, "-00\xc2\xb0"); x += 4;
    snprintf(buf, sizeof buf, "%3.0f\xc2\xb0", cam.Fov());
    put("F", lbl); put(buf, val, "000\xc2\xb0"); x += 4;
    snprintf(buf, sizeof buf, "%5.1f", cam.Distance());
    put("D", lbl); put(buf, val, "000.0"); div();

    // Frame + Stats
    put(fn, IM_COL32(75, 140, 190, 255)); div();
    snprintf(buf, sizeof buf, "%ddc %dv", sc.stats.drawCalls, sc.stats.vertices);
    put(buf, lbl);
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

// Jacobi eigensolver — diagonalize 3x3 symmetric matrix
static void Eigen3(const glm::mat3& A, glm::vec3& eigenvalues, glm::mat3& eigenvectors) {
    glm::mat3 D = A;
    eigenvectors = glm::mat3(1.f);
    for (int iter = 0; iter < 50; ++iter) {
        int p = 0, q = 1;
        float mx = std::abs(D[0][1]);
        if (std::abs(D[0][2]) > mx) { p = 0; q = 2; mx = std::abs(D[0][2]); }
        if (std::abs(D[1][2]) > mx) { p = 1; q = 2; }
        if (mx < 1e-8f) break;
        float theta = 0.5f * std::atan2(2.f * D[p][q], D[q][q] - D[p][p]);
        float c = std::cos(theta), s = std::sin(theta);
        glm::mat3 J(1.f);
        J[p][p] = c;  J[q][q] = c;
        J[p][q] = s;  J[q][p] = -s;
        D = glm::transpose(J) * D * J;
        eigenvectors = eigenvectors * J;
    }
    eigenvalues = {D[0][0], D[1][1], D[2][2]};
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

void Sensor(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up,
            float fovDeg, float aspect, float len,
            const glm::vec4& color, float width) {
    PickGroup pg;
    glm::vec3 fwd   = glm::normalize(dir);
    glm::vec3 c     = glm::cross(fwd, up);
    if (glm::dot(c, c) < 1e-8f) return;  // degenerate: dir parallel to up
    glm::vec3 right = glm::normalize(c);
    glm::vec3 camUp = glm::cross(right, fwd);

    float halfH = len * std::tan(glm::radians(fovDeg * 0.5f));
    float halfW = halfH * aspect;
    glm::vec3 center = pos + fwd * len;

    // Image plane corners
    glm::vec3 tl = center - right * halfW + camUp * halfH;
    glm::vec3 tr = center + right * halfW + camUp * halfH;
    glm::vec3 bl = center - right * halfW - camUp * halfH;
    glm::vec3 br = center + right * halfW - camUp * halfH;

    // Frustum edges
    Line(pos, tl, color, width);
    Line(pos, tr, color, width);
    Line(pos, bl, color, width);
    Line(pos, br, color, width);

    // Image plane rectangle
    Line(tl, tr, color, width);
    Line(tr, br, color, width);
    Line(br, bl, color, width);
    Line(bl, tl, color, width);

    // Up-direction triangle
    float triH = halfW * 0.35f;
    glm::vec3 tb1 = tl + (tr - tl) * 0.35f;
    glm::vec3 tb2 = tl + (tr - tl) * 0.65f;
    glm::vec3 tip = (tb1 + tb2) * 0.5f + camUp * triH;
    Line(tb1, tb2, color, width);
    Line(tb1, tip, color, width);
    Line(tb2, tip, color, width);
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

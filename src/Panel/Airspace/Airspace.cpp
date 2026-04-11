#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include "Ui/IconsFontAwesome7.h"
#include <imgui.h>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {
    Render::GetCamera("flight").ResetFollow(12.f);
    waypoints_.Add(aircraft_.lat + 1.0 / 111.32, aircraft_.lon, 0.0);
}

// ── controls panel ─────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");

    aircraft_.DrawControls();
    ImGui::Separator();

    gimbal_.DrawControls(aircraft_, terrain_);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Waypoints"))
        waypoints_.DrawControls();
    ImGui::Separator();

    auto& cam = Render::GetCamera("flight");
    ImGui::Text("Camera: %s  [C]", cameraFree_ ? "FreeCam" : "Chase");
    ImGui::Text("Eye: %.1f, %.1f, %.1f  Dist: %.1f",
        cam.Position().x, cam.Position().y, cam.Position().z, cam.Distance());

    if (ImGui::CollapsingHeader("Globe"))
        DrawGlobeControls();

    if (ImGui::CollapsingHeader("Terrain"))
        terrain_.DrawControls(aircraft_.lat, aircraft_.lon);

    ImGui::End();
}

void Airspace::DrawGlobeControls() {
    auto& g = Render::GetGlobe("flight");
    ImGui::Checkbox("Lighting", &g.lighting);
    ImGui::SliderFloat("Ambient", &g.ambient, 0.f, 1.f);
    ImGui::ColorEdit3("Surface", &g.surfaceColor.x);
    ImGui::ColorEdit3("Grid",    &g.gratColor.x);
    ImGui::Separator();
    ImGui::Text("Atmosphere");
    ImGui::ColorEdit3("Atmo Color", &g.atmosphereColor.x);
    ImGui::SliderFloat("Atmo Power", &g.atmospherePow, 1.f, 10.f);
    ImGui::SliderFloat("Atmo Str",   &g.atmosphereStr, 0.f, 2.f);
    ImGui::Separator();
    ImGui::Text("Fog");
    ImGui::Checkbox("Fog", &g.fog);
    ImGui::ColorEdit3("Fog Color", &g.fogColor.x);
    ImGui::DragFloat("Fog Start", &g.fogStart, 100.f, 0.f, 100000.f, "%.0f m");
    ImGui::DragFloat("Fog End",   &g.fogEnd, 1000.f, 1000.f, 500000.f, "%.0f m");
    ImGui::Separator();
    ImGui::Text("Grid Fades (m)");
    ImGui::DragFloat("0.0001\xc2\xb0", &g.gridFades.x, 10.f,    50.f,    5000.f, "%.0f");
    ImGui::DragFloat("0.001\xc2\xb0",  &g.gridFades.y, 100.f,  100.f,   50000.f, "%.0f");
    ImGui::DragFloat("0.01\xc2\xb0",   &g.gridFades.z, 1000.f, 1000.f, 500000.f, "%.0f");
    ImGui::DragFloat("0.1\xc2\xb0",    &g.gridFades.w, 5000.f, 5000.f, 2000000.f, "%.0f");
}

// ── shared world (drawn in both flight and gimbal scenes) ──────

void Airspace::DrawWorld(const glm::vec3& aircraftPosNed) {
    targetOnWaypoint_ = terrain_.Ready()
        ? waypoints_.Draw(aircraftPosNed, gimbal_, terrain_, rightOnMarker_)
        : false;
    aircraft_.DrawAt(aircraftPosNed);
}

// ── flight view ────────────────────────────────────────────────

void Airspace::DrawFlightView(float dt) {
    SetupEnv("flight");
    auto nedPos = glm::vec3(Render::GeoToLocal("flight", aircraft_.lat, aircraft_.lon, aircraft_.alt));

    // Camera mode toggle (C key) — chase follows heading, freecam = orbit
    auto& cam = Render::GetCamera("flight");
    bool focused = ImGui::IsWindowFocused();
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        cameraFree_ = !cameraFree_;
        if (!cameraFree_) cam.ResetFollow();
    }
    if (!cameraFree_) cam.Follow(nedPos, aircraft_.yaw);
    else              cam.Unfollow();

    aircraft_.HandleInput(dt, focused && !cameraFree_);

    Render::Begin("flight");
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();
        terrain_.RebuildIfNeeded(aircraft_.lat, aircraft_.lon);
        terrain_.Draw();

        DrawWorld(nedPos);
        if (Render::Event().Hovered())
            Render::Text(nedPos + glm::vec3(0, 0, -0.5f), {1,1,1,.5f},
                "Lat %.6f\nLon %.6f\nAlt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);

        // Ground projection (cross at surface, vertical line to aircraft)
        Render::Cross({nedPos.x, nedPos.y, 0.f}, 0.5f, {1,1,1,.5f}, 2.f);
        Render::Line(nedPos, {nedPos.x, nedPos.y, 0.f}, {1,1,1,.15f}, 1.0f);

        trail_.Draw({.5f, .5f, .55f, .4f}, 1.5f);

        // Gimbal: precompute positions, share between frustum + target marker
        auto gimbalNed = gimbal_.PositionFrom(nedPos, aircraft_);
        auto targetNed = gimbal_.TargetInScene("flight");
        gimbal_.DrawFrustum(nedPos, gimbalNed, targetNed, aircraft_);

        // Standalone target marker — skip when target coincides with a waypoint
        if (!targetOnWaypoint_) {
            Render::Group tgtGroup;
            Render::Marker(targetNed, ICON_FA_CROSSHAIRS, "Target", Render::Color::Hex("#00ccffff"),
                "Lat %.6f\nLon %.6f\nAlt %.0f m", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            if (Render::Event().Dragging()) {
                auto& io = ImGui::GetIO();
                double lat, lon, alt;
                if (terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt))
                    gimbal_.SetTarget(lat, lon, alt);
            }
        }
    Render::End();
    Render::StatusBar();
    Render::Overlay();
        ImGui::TextColored({1,1,1,.5f}, "Speed = %.1f km/h", aircraft_.speed * 3.6f);
    Render::OverlayEnd();

    if (!terrain_.Ready()) {
        auto vp = ImGui::GetWindowPos();
        auto sz = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos({vp.x + sz.x * 0.5f - 60.f, vp.y + sz.y * 0.5f});
        ImGui::TextColored({0.6f, 0.8f, 1.f, 1.f}, "Loading terrain...");
    } else {
        // Double-click → add waypoint at terrain surface
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            auto& io = ImGui::GetIO();
            double lat, lon, alt;
            if (terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt))
                waypoints_.Add(lat, lon, alt);
        }
        // Reset marker flag on right release
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            rightOnMarker_ = false;
        // Right-hold on terrain → continuously update gimbal target
        if (!rightOnMarker_ && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            auto& io = ImGui::GetIO();
            double lat, lon, alt;
            if (terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt))
                gimbal_.SetTarget(lat, lon, alt);
        }
    }

    if (!cameraFree_) cam.CaptureFollow();
}

// ── gimbal POV view ────────────────────────────────────────────

void Airspace::DrawGimbalView() {
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");

        auto aircraftNed = glm::vec3(Render::GeoToLocal("gimbal",
            aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalNed   = gimbal_.PositionFrom(aircraftNed, aircraft_);
        auto targetNed   = gimbal_.TargetInScene("gimbal");
        float dist       = glm::length(targetNed - gimbalNed);

        auto& cam = Render::GetCamera("gimbal");
        cam.LookAt(gimbalNed, targetNed);
        cam.Fov()       = gimbal_.fov;
        cam.NearPlane() = 0.05f;

        Render::Begin("gimbal");
            Render::SetFrame(Render::FrameId::NED);
            Render::Globe();
            terrain_.RebuildIfNeeded(aircraft_.lat, aircraft_.lon);
            terrain_.Draw();
            DrawWorld(aircraftNed);
        Render::End();
        Render::Crosshair();

        if (Render::Overlay()) {
            ImGui::TextColored({1,1,1,.4f}, "FOV %.0f\xc2\xb0  D %.0fm", gimbal_.fov, dist);
            ImGui::TextColored({1,1,1,.3f}, "%.6f  %.6f  %.0fm",
                gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            Render::OverlayEnd();
        }
    ImGui::End();
}

void Airspace::SetupEnv(const char* scene) {
    Render::SetOrigin(scene, aircraft_.lat, aircraft_.lon, 0.0);
    auto& env    = Render::GetEnvironment(scene);
    env.bgColor  = {0.015f, 0.02f, 0.04f};
    env.showSun  = true;
    env.lightDir = Render::ToInternal<Render::NED>({0.3f, 0.2f, -0.9f});
}

// ── lifecycle ──────────────────────────────────────────────────

void Airspace::OnLoop() {
    constexpr float kDt = 0.001f;  // 1ms fixed timestep (worker thread)
    aircraft_.UpdatePhysics(kDt);
    trail_.Record(aircraft_.lat, aircraft_.lon, aircraft_.alt);
}

void Airspace::OnDraw() {
    bool wasReady = terrain_.Ready();
    terrain_.Poll();
    if (!wasReady && terrain_.Ready()) {
        // First terrain-ready frame: snap everything to surface
        aircraft_.alt     = double(terrain_.Sample(aircraft_.lat, aircraft_.lon)) + 50.0;
        gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
        waypoints_.SnapToTerrain(terrain_);
        terrain_.RebuildIfNeeded(aircraft_.lat, aircraft_.lon, true);
    }

    DrawControls();
    DrawFlightView(ImGui::GetIO().DeltaTime);
    DrawGimbalView();
}

// ── persistence ────────────────────────────────────────────────

json Airspace::SaveSettings() const {
    return {
        {"waypoints", waypoints_.Save()},
        {"terrain",   terrain_.Save()},
    };
}

void Airspace::LoadSettings(const json& j) {
    if (j.contains("terrain"))   terrain_.Load(j["terrain"]);
    if (j.contains("waypoints")) waypoints_.Load(j["waypoints"]);
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

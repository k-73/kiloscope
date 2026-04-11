#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include <imgui.h>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {
    Render::GetCamera("flight").ResetFollow(12.f);
    waypoints_.Add(aircraft_.lat + 1.0 / 111.32, aircraft_.lon, 0.0);
}

// ── lifecycle ──────────────────────────────────────────────────

void Airspace::OnLoop() {
    constexpr float kDt = 0.001f;  // 1 ms fixed timestep (worker thread)
    aircraft_.UpdatePhysics(kDt);
    trail_.Record(aircraft_.lat, aircraft_.lon, aircraft_.alt);
}

void Airspace::OnDraw() {
    if (terrain_.Poll()) OnTerrainReady();
    if (terrain_.Ready()) terrain_.RebuildIfNeeded(aircraft_.lat, aircraft_.lon);

    DrawControls();
    DrawFlightView(ImGui::GetIO().DeltaTime);
    DrawGimbalView();
}

// First terrain-ready frame: snap aircraft/target/waypoints to surface.
void Airspace::OnTerrainReady() {
    aircraft_.alt     = double(terrain_.Sample(aircraft_.lat, aircraft_.lon)) + 50.0;
    gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
    waypoints_.SnapToTerrain(terrain_);
    terrain_.RebuildIfNeeded(aircraft_.lat, aircraft_.lon, true);
}

// ── flight view ────────────────────────────────────────────────

void Airspace::DrawFlightView(float dt) {
    SetupEnv("flight");
    auto aircraftNed = glm::vec3(Render::GeoToLocal("flight",
        aircraft_.lat, aircraft_.lon, aircraft_.alt));

    UpdateFlightCamera(aircraftNed);
    HandleFlightInput(dt);
    DrawFlightScene(aircraftNed);
    DrawFlightOverlays();
    HandleFlightMouse();

    if (!cameraFree_) Render::GetCamera("flight").CaptureFollow();
}

// Chase camera follows aircraft heading; freecam orbits freely.
void Airspace::UpdateFlightCamera(const glm::vec3& aircraftNed) {
    auto& cam = Render::GetCamera("flight");
    if (!cameraFree_) cam.Follow(aircraftNed, aircraft_.yaw);
    else              cam.Unfollow();
}

// C key toggles camera mode; aircraft takes keyboard only when focused + in chase.
void Airspace::HandleFlightInput(float dt) {
    bool focused = ImGui::IsWindowFocused();
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        cameraFree_ = !cameraFree_;
        if (!cameraFree_) Render::GetCamera("flight").ResetFollow();
    }
    aircraft_.HandleInput(dt, focused && !cameraFree_);
}

// Terrain-dependent mouse actions (ray-casted to surface) — run after scene render.
void Airspace::HandleFlightMouse() {
    if (!terrain_.Ready()) return;

    auto& io = ImGui::GetIO();
    auto raycast = [&](double& lat, double& lon, double& alt) {
        return terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt);
    };

    // Double-click → add waypoint at surface
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        double lat, lon, alt;
        if (raycast(lat, lon, alt)) waypoints_.Add(lat, lon, alt);
    }

    // Right-hold on terrain → continuously update gimbal target
    // (suppressed when the press started on a marker)
    bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (!rmb) rightOnMarker_ = false;
    if (rmb && !rightOnMarker_) {
        double lat, lon, alt;
        if (raycast(lat, lon, alt)) gimbal_.SetTarget(lat, lon, alt);
    }
}

void Airspace::DrawFlightScene(const glm::vec3& aircraftNed) {
    Render::Begin("flight");
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();
        terrain_.Draw();

        DrawWorld(aircraftNed);

        // Hover tooltip with geodetic position
        if (Render::Event().Hovered())
            Render::Text(aircraftNed + glm::vec3(0, 0, -0.5f), {1,1,1,.5f},
                "Lat %.6f\nLon %.6f\nAlt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);

        // Ground projection (surface cross + vertical drop line)
        Render::Cross({aircraftNed.x, aircraftNed.y, 0.f}, 0.5f, {1,1,1,.5f}, 2.f);
        Render::Line(aircraftNed, {aircraftNed.x, aircraftNed.y, 0.f}, {1,1,1,.15f}, 1.f);

        trail_.Draw({.5f, .5f, .55f, .4f}, 1.5f);

        gimbal_.DrawFrustum(aircraftNed, aircraft_);
        if (!targetOnWaypoint_) gimbal_.DrawTargetMarker(terrain_);
    Render::End();
}

void Airspace::DrawFlightOverlays() {
    Render::StatusBar();
    Render::Overlay();
        ImGui::TextColored({1,1,1,.5f}, "Speed = %.1f km/h", aircraft_.speed * 3.6f);
    Render::OverlayEnd();

    if (!terrain_.Ready()) {
        auto vp = ImGui::GetWindowPos();
        auto sz = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos({vp.x + sz.x * 0.5f - 60.f, vp.y + sz.y * 0.5f});
        ImGui::TextColored({0.6f, 0.8f, 1.f, 1.f}, "Loading terrain...");
    }
}

// ── gimbal POV view ────────────────────────────────────────────

void Airspace::DrawGimbalView() {
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");

        auto aircraftNed = glm::vec3(Render::GeoToLocal("gimbal",
            aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalNed   = gimbal_.PositionFrom(aircraftNed, aircraft_);
        auto targetNed   = gimbal_.TargetInScene("gimbal");

        UpdateGimbalCamera(gimbalNed, targetNed);
        DrawGimbalScene(aircraftNed);
        DrawGimbalOverlay(glm::length(targetNed - gimbalNed));
    ImGui::End();
}

void Airspace::UpdateGimbalCamera(const glm::vec3& gimbalNed, const glm::vec3& targetNed) {
    auto& cam = Render::GetCamera("gimbal");
    cam.LookAt(gimbalNed, targetNed);
    cam.Fov()       = gimbal_.fov;
    cam.NearPlane() = 0.05f;
}

void Airspace::DrawGimbalScene(const glm::vec3& aircraftNed) {
    Render::Begin("gimbal");
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();
        terrain_.Draw();
        DrawWorld(aircraftNed);
    Render::End();
    Render::Crosshair();
}

void Airspace::DrawGimbalOverlay(float distance) {
    if (!Render::Overlay()) return;
    ImGui::TextColored({1,1,1,.4f}, "FOV %.0f\xc2\xb0  D %.0fm", gimbal_.fov, distance);
    ImGui::TextColored({1,1,1,.3f}, "%.6f  %.6f  %.0fm",
        gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
    Render::OverlayEnd();
}

// ── shared world content (drawn in both scenes) ────────────────

void Airspace::DrawWorld(const glm::vec3& aircraftNed) {
    targetOnWaypoint_ = terrain_.Ready()
        ? waypoints_.Draw(aircraftNed, gimbal_, terrain_, rightOnMarker_)
        : false;
    aircraft_.DrawAt(aircraftNed);
}

// ── controls UI ────────────────────────────────────────────────

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

// ── scene setup ────────────────────────────────────────────────

void Airspace::SetupEnv(const char* scene) {
    Render::SetOrigin(scene, aircraft_.lat, aircraft_.lon, 0.0);
    auto& env    = Render::GetEnvironment(scene);
    env.bgColor  = {0.015f, 0.02f, 0.04f};
    env.showSun  = true;
    env.lightDir = Render::ToInternal<Render::NED>({0.3f, 0.2f, -0.9f});
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

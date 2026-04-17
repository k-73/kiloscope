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
    constexpr float kDt = 0.001f;
    aircraft_.UpdatePhysics(kDt);
    trail_.Record(aircraft_.lat, aircraft_.lon, aircraft_.alt);
}

void Airspace::OnDraw() {
    if (terrain_.Poll())  OnTerrainReady();
    if (terrain_.Ready()) terrain_.RebuildIfNeeded();

    DrawControls();
    DrawFlightView(ImGui::GetIO().DeltaTime);
    DrawGimbalView();
}

void Airspace::OnTerrainReady() {
    aircraft_.alt     = double(terrain_.Sample(aircraft_.lat, aircraft_.lon)) + 50.0;
    gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
    waypoints_.SnapToTerrain(terrain_);
    terrain_.RebuildIfNeeded(true);
}

// ── flight view ────────────────────────────────────────────────

void Airspace::DrawFlightView(float dt) {
    SetupEnv("flight");
    auto aircraftNed = glm::vec3(Render::GeoToLocal("flight",
        aircraft_.lat, aircraft_.lon, aircraft_.alt));

    auto& cam = Render::GetCamera("flight");
    bool focused = ImGui::IsWindowFocused();
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        cameraFree_ = !cameraFree_;
        if (!cameraFree_) cam.ResetFollow();
    }
    if (!cameraFree_) cam.Follow(aircraftNed, aircraft_.yaw);
    else              cam.Unfollow();
    aircraft_.HandleInput(dt, focused && !cameraFree_);

    Render::Begin("flight");
        Render::SetFrame(Render::FrameId::NED);
        DrawWorld(aircraftNed);

        // Waypoints are interactive only in the flight scene.
        waypointEvents_ = terrain_.Ready()
            ? waypoints_.Draw(aircraftNed,
                              {gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt},
                              terrain_)
            : Waypoints::DrawResult{};

        // Sync gimbal target from waypoint events:
        //   • right-click on any marker  → that waypoint becomes the target
        //   • drag on the target marker  → target follows the drag
        if (waypointEvents_.rightClickedIdx >= 0) {
            const auto& wp = waypoints_.list[waypointEvents_.rightClickedIdx];
            gimbal_.SetTarget(wp.lat, wp.lon, wp.alt);
            rmbOnMarker_ = true;
        } else if (waypointEvents_.targetDragged) {
            const auto& wp = waypoints_.list[waypointEvents_.draggedIdx];
            gimbal_.SetTarget(wp.lat, wp.lon, wp.alt);
        }

        if (Render::Event().Hovered())
            Render::Text(aircraftNed + glm::vec3(0, 0, -0.5f), {1,1,1,.5f},
                "Lat %.6f\nLon %.6f\nAlt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);

        Render::Cross({aircraftNed.x, aircraftNed.y, 0.f}, 0.5f, {1,1,1,.5f}, 2.f);
        Render::Line(aircraftNed, {aircraftNed.x, aircraftNed.y, 0.f}, {1,1,1,.15f}, 1.f);
        trail_.Draw({.5f, .5f, .55f, .4f}, 1.5f);

        gimbal_.DrawFrustum(aircraftNed);
        if (!waypointEvents_.targetMatched) gimbal_.DrawTargetMarker();
    Render::End();

    Render::StatusBar();
    Render::Overlay();
        ImGui::TextColored({1,1,1,.5f}, "Speed = %.1f km/h", aircraft_.speed * 3.6f);
    Render::OverlayEnd();

    if (!terrain_.Ready()) {
        auto windowPos  = ImGui::GetWindowPos();
        auto windowSize = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos({windowPos.x + windowSize.x * 0.5f - 60.f,
                                   windowPos.y + windowSize.y * 0.5f});
        ImGui::TextColored({0.6f, 0.8f, 1.f, 1.f}, "Loading terrain...");
    }

    HandleFlightMouse();
    if (!cameraFree_) cam.CaptureFollow();
}

void Airspace::HandleFlightMouse() {
    if (!terrain_.Ready()) return;

    auto& io = ImGui::GetIO();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) rmbOnMarker_ = false;

    auto pickGround = [&](double& lat, double& lon, double& alt) {
        return terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt, "flight");
    };

    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        double lat, lon, alt;
        if (pickGround(lat, lon, alt)) waypoints_.Add(lat, lon, alt);
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && !rmbOnMarker_) {
        double lat, lon, alt;
        if (pickGround(lat, lon, alt)) gimbal_.SetTarget(lat, lon, alt);
    }
}

// ── gimbal POV view ────────────────────────────────────────────

void Airspace::DrawGimbalView() {
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");
        auto aircraftNed = glm::vec3(Render::GeoToLocal("gimbal",
            aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalPos = gimbal_.PositionNed(aircraftNed);
        auto targetPos = gimbal_.TargetNed("gimbal");

        auto& cam = Render::GetCamera("gimbal");
        cam.LookAt(gimbalPos, targetPos);
        cam.Fov()       = gimbal_.fov;
        cam.NearPlane() = 0.05f;

        Render::Begin("gimbal");
            Render::SetFrame(Render::FrameId::NED);
            DrawWorld(aircraftNed);
            if (terrain_.Ready())
                waypoints_.Draw(aircraftNed,
                                {gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt},
                                terrain_);
        Render::End();
        Render::Crosshair();

        if (Render::Overlay()) {
            float distance = glm::length(targetPos - gimbalPos);
            ImGui::TextColored({1,1,1,.4f}, "FOV %.0f\xc2\xb0  D %.0fm", gimbal_.fov, distance);
            ImGui::TextColored({1,1,1,.3f}, "%.6f  %.6f  %.0fm",
                gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            Render::OverlayEnd();
        }
    ImGui::End();
}

// ── shared world content ──────────────────────────────────────

void Airspace::DrawWorld(const glm::vec3& aircraftNed) {
    Render::Globe();
    terrain_.Draw();
    aircraft_.DrawAt(aircraftNed);
}

// ── controls UI ────────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");

    aircraft_.DrawControls();
    ImGui::Separator();

    gimbal_.DrawControls();
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Waypoints"))
        waypoints_.DrawControls();
    ImGui::Separator();

    auto& cam = Render::GetCamera("flight");
    ImGui::Text("Camera: %s  [C]", cameraFree_ ? "FreeCam" : "Chase");
    ImGui::Text("Eye: %.1f, %.1f, %.1f  Dist: %.1f",
        cam.Position().x, cam.Position().y, cam.Position().z, cam.Distance());

    if (ImGui::CollapsingHeader("Globe"))
        Render::GlobeControls(Render::GetGlobe("flight"));

    if (ImGui::CollapsingHeader("Terrain"))
        terrain_.DrawControls();

    ImGui::End();
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

#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Model.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include <GeographicLib/Geocentric.hpp>
#include "Ui/Widget.hpp"
#include "Ui/IconsFontAwesome7.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {
    Render::GetCamera("flight").ResetFollow(12.f);
}

// ── aircraft model ──────────────────────────────────────────────

static Render::ModelId sJetModel = Render::kInvalidModel;

static void DrawAircraft() {
    if (sJetModel == Render::kInvalidModel)
        sJetModel = Render::LoadModel(std::string(ASSETS_DIR) + "/models/Jet_Lowpoly.obj");

    Render::Group group;
    Render::PushMatrix();
        Render::Scale(0.33f);
        Render::Translate(0, 0, -1.0f); // model is centered on canopy, move origin to landing gear
        // OBJ (X=right, Y=up, +Z=forward) → Body (X=forward, Y=right, Z=down)
        Render::Transform(glm::mat4(
            glm::vec4(0, -1, 0, 0),  // OBJ X → Body -Y
            glm::vec4(0, 0, -1, 0),  // OBJ Y → Body -Z
            glm::vec4(1, 0, 0, 0),   // OBJ Z → Body +X
            glm::vec4(0, 0, 0, 1)));
        Render::Model(sJetModel, Render::Color::Hex("#3a5570"));
    Render::PopMatrix();
}

// ── controls ────────────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");

    // Aircraft
    ImGui::Text("Lat %.6f  Lon %.6f  Alt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);
    ImGui::DragFloat("Speed", &aircraft_.speed, 0.5f, 0.f, 200.f, "%.1f m/s");
    ImGui::SliderFloat("Yaw",   &aircraft_.yaw,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &aircraft_.pitch, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::Separator();

    // Gimbal
    ImGui::SliderFloat("FOV", &gimbal_.fov, 5.f, 120.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Aspect", &gimbal_.aspect, 0.5f, 3.f, "%.2f");
    ImGui::InputDouble("Target Lat", &gimbal_.targetLat, 0.01, 0.1, "%.6f");
    ImGui::InputDouble("Target Lon", &gimbal_.targetLon, 0.01, 0.1, "%.6f");
    ImGui::InputDouble("Target Alt", &gimbal_.targetAlt, 1.0, 10.0, "%.0f");

    glm::vec2 joy;
    if (Widget::Joystick("##gimbal", &joy)) {
        float dt   = ImGui::GetIO().DeltaTime;
        float rate = 0.0002f;
        float fx   = joy.x * std::abs(joy.x) * rate * dt;
        float fy   = -joy.y * std::abs(joy.y) * rate * dt;
        float yr   = glm::radians(aircraft_.yaw);
        float cosLat = std::cos(glm::radians(float(aircraft_.lat)));
        gimbal_.targetLat += fy * std::cos(yr) - fx * std::sin(yr);
        gimbal_.targetLon += (fy * std::sin(yr) + fx * std::cos(yr)) / std::max(cosLat, 0.01f);
    }
    ImGui::Separator();

    // Camera
    auto& cam = Render::GetCamera("flight");
    ImGui::Text("Camera: %s  [C]", cameraMode_.free ? "FreeCam" : "Chase");
    ImGui::Text("Eye: %.1f, %.1f, %.1f  Dist: %.1f",
        cam.Position().x, cam.Position().y, cam.Position().z, cam.Distance());

    // Globe
    if (ImGui::CollapsingHeader("Globe")) {
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
        ImGui::DragFloat("0.0001\xc2\xb0", &g.gridFades.x, 10.f, 50.f, 5000.f, "%.0f");
        ImGui::DragFloat("0.001\xc2\xb0",  &g.gridFades.y, 100.f, 100.f, 50000.f, "%.0f");
        ImGui::DragFloat("0.01\xc2\xb0",   &g.gridFades.z, 1000.f, 1000.f, 500000.f, "%.0f");
        ImGui::DragFloat("0.1\xc2\xb0",    &g.gridFades.w, 5000.f, 5000.f, 2000000.f, "%.0f");
    }

    ImGui::End();
}

// ── input ───────────────────────────────────────────────────────

void Airspace::HandleInput(float dt, bool focused) {
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        cameraMode_.free = !cameraMode_.free;
        if (!cameraMode_.free) Render::GetCamera("flight").ResetFollow();
    }
    if (!focused || cameraMode_.free) return;

    constexpr float kPitchRate = 40.f, kYawRate = 50.f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) aircraft_.pitch -= kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_S)) aircraft_.pitch += kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_A)) aircraft_.yaw   -= kYawRate   * dt;
    if (ImGui::IsKeyDown(ImGuiKey_D)) aircraft_.yaw   += kYawRate   * dt;
}

// ── physics ─────────────────────────────────────────────────────

void Airspace::UpdatePhysics(float dt) {
    using GR = Render::GeoRef;
    aircraft_.pitch = std::clamp(aircraft_.pitch, -80.f, 80.f);

    // Velocity components in local NED
    double yr = glm::radians(double(aircraft_.yaw));
    double pr = glm::radians(double(aircraft_.pitch));
    double dN = aircraft_.speed * std::cos(yr) * std::cos(pr) * dt;  // North
    double dE = aircraft_.speed * std::sin(yr) * std::cos(pr) * dt;  // East
    double dU = aircraft_.speed * std::sin(pr) * dt;                  // Up

    // ECEF integration (correct at all latitudes including poles)
    auto ecef = GR::ToEcef(aircraft_.lat, aircraft_.lon, aircraft_.alt);
    double phi = glm::radians(aircraft_.lat), lam = glm::radians(aircraft_.lon);
    double sp = std::sin(phi), cp = std::cos(phi);
    double sl = std::sin(lam), cl = std::cos(lam);

    // NED basis vectors in ECEF
    glm::dvec3 N{-sp * cl, -sp * sl,  cp};   // North
    glm::dvec3 E{-sl,       cl,        0.0};  // East
    glm::dvec3 U{ cp * cl,  cp * sl,   sp};   // Up

    ecef += N * dN + E * dE + U * dU;

    // ECEF → geodetic via GeographicLib (7nm precision, correct at poles)
    static const auto& earth = GeographicLib::Geocentric::WGS84();
    earth.Reverse(ecef.x, ecef.y, ecef.z, aircraft_.lat, aircraft_.lon, aircraft_.alt);
    aircraft_.alt = std::max(aircraft_.alt, 1.0);

    // Bank autopilot
    float bank = 0.f;
    if (ImGui::IsKeyDown(ImGuiKey_D)) bank += 1.f;
    if (ImGui::IsKeyDown(ImGuiKey_A)) bank -= 1.f;
    aircraft_.roll += (bank * 35.f - aircraft_.roll) * std::min(1.f, 5.f * dt);
}

// ── scene ───────────────────────────────────────────────────────

void Airspace::DrawWorld(const glm::vec3& pos) {
    // Waypoint — fixed position, 1km north of start
    static const double wpLat = 52.2297 + 1.0 / 111.32, wpLon = 21.0122;
    auto wpNed = glm::vec3(Render::GeoToLocal("flight", wpLat, wpLon, 0.0));
    float wpDist = glm::length(wpNed - pos) * 0.001f;
    Render::Marker(wpNed, ICON_FA_LOCATION_DOT, "WP1", Render::Color::Orange,
        "%.2f km\n%.6f, %.6f", wpDist, wpLat, wpLon);

    Render::PushMatrix();
        Render::Translate(pos);
        Render::RotateZ(aircraft_.yaw);
        Render::RotateY(aircraft_.pitch);
        Render::RotateX(aircraft_.roll);
        DrawAircraft();
    Render::PopMatrix();
}

void Airspace::DrawFlight() {
    SetupEnv("flight");
    auto nedPos = glm::vec3(Render::GeoToLocal("flight", aircraft_.lat, aircraft_.lon, aircraft_.alt));

    auto& cam = Render::GetCamera("flight");
    if (!cameraMode_.free && cameraMode_.chase)
        cam.Follow(nedPos, aircraft_.yaw);
    else
        cam.Unfollow();

    Render::Begin("flight");
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();
        DrawWorld(nedPos);
        auto aircraftEv = Render::Event();
        if (aircraftEv.Hovered()) {
            Render::Text(nedPos + glm::vec3(0, 0, -0.5f), {1,1,1,0.5f}, "Lat %.6f\nLon %.6f\nAlt %.0f m",
                aircraft_.lat, aircraft_.lon, aircraft_.alt);
        }

        Render::Cross({nedPos.x, nedPos.y, 0.f}, 0.5f, {1,1,1,.5f}, 2.f);
        Render::Line(nedPos, {nedPos.x, nedPos.y, 0.f}, {1,1,1,.15f}, 1.0f);

        if (trail_.size() > 1) {
            trailBuf_.resize(trail_.size());
            for (size_t i = 0; i < trail_.size(); ++i)
                trailBuf_[i] = glm::vec3(Render::GeoToLocal(trail_[i].lat, trail_[i].lon, trail_[i].alt));
            Render::Trail(trailBuf_.data(), int(trailBuf_.size()), Render::Color::Hex("#FFD700"), 2.f);
        }

        auto gimbalNed = nedPos + BodyToNed() * gimbal_.bodyOffset;
        auto targetNed = glm::vec3(Render::GeoToLocal("flight", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt));
        auto gimbalDir = glm::normalize(targetNed - gimbalNed);
        Render::Line(nedPos, gimbalNed, Render::Color::Hex("#d0b09050"), 1.f);
        Render::Sensor(gimbalNed, gimbalDir, {0,0,-1}, gimbal_.fov, gimbal_.aspect, 0.1,
            Render::Color::Hex("#90B0D0"), 1.0f);
        Render::Line(gimbalNed, targetNed, Render::Color::Hex("#90B0D050"), 1.f);
        Render::Marker(targetNed, ICON_FA_CROSSHAIRS, "Target", Render::Color::Hex("#00ccff"),
            "Lat %.6f\nLon %.6f\nAlt %.0f m", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
    Render::End();
    Render::HUD();

    if (!cameraMode_.free && cameraMode_.chase)
        cam.CaptureFollow();

    Render::GeoCoord gc{aircraft_.lat, aircraft_.lon, aircraft_.alt};
    if (trail_.empty() || std::abs(gc.lat - trail_.back().lat) > 1e-7
                       || std::abs(gc.lon - trail_.back().lon) > 1e-7) {
        trail_.push_back(gc);
        if (trail_.size() > kTrailMax) trail_.erase(trail_.begin());
    }
}

void Airspace::DrawGimbal() {
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");
        auto aircraftNed = glm::vec3(Render::GeoToLocal("gimbal", aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalNed   = aircraftNed + BodyToNed() * gimbal_.bodyOffset;
        auto targetNed   = glm::vec3(Render::GeoToLocal("gimbal", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt));

        auto& cam = Render::GetCamera("gimbal");
        cam.LookAt(gimbalNed, targetNed);
        cam.Fov() = gimbal_.fov;
        cam.NearPlane() = 0.05f;

        float dist = glm::length(targetNed - gimbalNed);

        Render::Begin("gimbal");
            Render::SetFrame(Render::FrameId::NED);
            Render::Globe();
            DrawWorld(aircraftNed);
        Render::End();
        Render::Crosshair();
        if (Render::HudBegin()) {
            ImGui::TextColored({1,1,1,.4f}, "FOV %.0f\xc2\xb0  D %.0fm", gimbal_.fov, dist);
            ImGui::TextColored({1,1,1,.3f}, "%.6f  %.6f  %.0fm",
                gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            Render::HudEnd();
        }
    ImGui::End();
}

void Airspace::SetupEnv(const char* scene) {
    // Origin = aircraft → all local coords small → float32 precise
    Render::SetOrigin(scene, aircraft_.lat, aircraft_.lon, 0.0);
    auto& env    = Render::GetEnvironment(scene);
    env.bgColor  = {0.015f, 0.02f, 0.04f};
    env.showSun  = true;
    env.lightDir = Render::ToInternal<Render::NED>({0.3f, 0.2f, -0.9f});
}

// ── orchestrator ────────────────────────────────────────────────

void Airspace::OnDraw() {
    const float dt = ImGui::GetIO().DeltaTime;

    DrawControls();
    bool focused = ImGui::IsWindowFocused();
    HandleInput(dt, focused);
    UpdatePhysics(dt);

    DrawFlight();
    DrawGimbal();
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

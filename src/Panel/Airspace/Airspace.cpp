#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include <GeographicLib/Geocentric.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {}

// ── aircraft model (body: X=forward, Y=right, Z=down) ──────────

static void DrawAircraft() {
    constexpr auto Body = "#344b61", Wing = "#4D6E8C", Fin = "#7A9CB8";

    // Fuselage
    Render::Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Cone    ({1.0f,  0, 0}, {1.6f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Sphere  ({-1.5f, 0, 0},               0.15f, Render::Color::Hex(Body), 12);

    // Wings
    constexpr float s = 2.2f;
    Render::Triangle({-0.1f, -s, 0}, {-0.1f, s, 0}, {0.5f, 0, 0}, Render::Color::Hex(Wing), true);
    Render::Triangle({-0.1f, -s, 0}, {-0.5f, 0, 0}, {-0.1f, s, 0}, Render::Color::Hex(Wing), true);

    // Stabilizers + fin
    Render::Triangle({-1.3f, -0.6f, 0}, {-1.3f, 0.6f, 0}, {-0.9f, 0, 0}, Render::Color::Hex(Wing), true);
    Render::Triangle({-1.4f, 0, 0}, {-1.0f, 0, 0}, {-1.25f, 0, -0.5f}, Render::Color::Hex(Fin), true);
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

    // Gimbal target
    ImGui::InputDouble("Gimbal Lat", &gimbal_.lat, 0.01, 0.1, "%.6f");
    ImGui::InputDouble("Gimbal Lon", &gimbal_.lon, 0.01, 0.1, "%.6f");
    ImGui::InputDouble("Gimbal Alt", &gimbal_.alt, 1.0, 10.0, "%.0f");
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
        if (g.lighting) ImGui::SliderFloat("Ambient", &g.ambient, 0.f, 1.f);
        ImGui::ColorEdit3("Surface", &g.surfaceColor.x);
        ImGui::ColorEdit3("Grid",    &g.gratColor.x);
        ImGui::Separator();
        ImGui::Text("Atmosphere");
        ImGui::ColorEdit3("Atmo Color", &g.atmosphereColor.x);
        ImGui::SliderFloat("Atmo Power", &g.atmospherePow, 1.f, 10.f);
        ImGui::SliderFloat("Atmo Str",   &g.atmosphereStr, 0.f, 2.f);
        ImGui::Separator();
        ImGui::Text("Grid Fades (m)");
        ImGui::DragFloat("0.001\xc2\xb0", &g.gridFades.x, 100.f, 100.f, 50000.f, "%.0f");
        ImGui::DragFloat("0.01\xc2\xb0",  &g.gridFades.y, 1000.f, 1000.f, 500000.f, "%.0f");
        ImGui::DragFloat("0.1\xc2\xb0",   &g.gridFades.z, 5000.f, 5000.f, 2000000.f, "%.0f");
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

void Airspace::DrawWorld(const char* scene, const glm::vec3& pos) {
    Render::Begin(scene);
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();

        // Aircraft at NED position with ZYX rotation
        Render::PushMatrix();
            Render::Translate(pos);
            Render::RotateZ(aircraft_.yaw);
            Render::RotateY(aircraft_.pitch);
            Render::RotateX(aircraft_.roll);
            Render::PushMatrix();
                Render::Scale(0.4f);
                DrawAircraft();
            Render::PopMatrix();
        Render::PopMatrix();

        // Ground track (0.1m above surface to avoid z-fighting with Globe)
        glm::vec3 ground{pos.x, pos.y, -0.1f};
        Render::Cross(ground, 0.3f, Render::Color::Hex("#FFFFFF30"), 1.5f);
        Render::Line(pos, ground, Render::Color::Hex("#FFFFFF15"), 1.f);

        // Trail (geodetic → local NED, reusing buffer)
        if (trail_.size() > 1) {
            trailBuf_.resize(trail_.size());
            for (size_t i = 0; i < trail_.size(); ++i)
                trailBuf_[i] = glm::vec3(Render::GeoToLocal(trail_[i].lat, trail_[i].lon, trail_[i].alt));
            Render::Trail(trailBuf_.data(), static_cast<int>(trailBuf_.size()),
                          Render::Color::Hex("#FFD700"), 2.f);
        }
    Render::End();
}

void Airspace::SetupEnv(const char* scene) {
    // Origin = aircraft → all local coords small → float32 precise
    Render::SetOrigin(scene, aircraft_.lat, aircraft_.lon, 0.0);
    auto& env    = Render::GetEnvironment(scene);
    env.bgColor  = {0.06f, 0.08f, 0.14f};
    env.showSun  = true;
    env.lightDir = Render::ToInternal<Render::NED>({0.4f, 0.2f, -0.8f});
}

// ── orchestrator ────────────────────────────────────────────────

void Airspace::OnDraw() {
    const float dt = ImGui::GetIO().DeltaTime;

    DrawControls();
    bool focused = ImGui::IsWindowFocused();
    HandleInput(dt, focused);
    UpdatePhysics(dt);

    // ── Main view ────────────────────────────────────────────────
    SetupEnv("flight");
    auto nedPos = glm::vec3(Render::GeoToLocal("flight", aircraft_.lat, aircraft_.lon, aircraft_.alt));

    auto& flightCam = Render::GetCamera("flight");
    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.Follow(nedPos, aircraft_.yaw);
    else
        flightCam.Unfollow();

    DrawWorld("flight", nedPos);

    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.CaptureFollow();

    // Record trail in geodetic
    Render::GeoCoord gc{aircraft_.lat, aircraft_.lon, aircraft_.alt};
    if (trail_.empty() || std::abs(gc.lat - trail_.back().lat) > 1e-7
                       || std::abs(gc.lon - trail_.back().lon) > 1e-7) {
        trail_.push_back(gc);
        if (trail_.size() > kTrailMax) trail_.erase(trail_.begin());
    }

    // ── Gimbal — mounted under aircraft, looking at target ───────
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");
        auto gimbalPos = glm::vec3(Render::GeoToLocal("gimbal",
            aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalTarget = glm::vec3(Render::GeoToLocal("gimbal",
            gimbal_.lat, gimbal_.lon, gimbal_.alt));

        auto& gimbalCam = Render::GetCamera("gimbal");
        gimbalCam.LookAt(gimbalPos + glm::vec3(0.f, 0.f, 0.3f), gimbalTarget);
        gimbalCam.Fov() = 50.f;

        DrawWorld("gimbal", gimbalPos);
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

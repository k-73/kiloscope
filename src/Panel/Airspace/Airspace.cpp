#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include "Render/DrawGlobe.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {}

// ── aircraft model (body frame: X=forward, Y=right, Z=down) ────

static void DrawAircraft() {
    constexpr auto Body = "#344b61";
    constexpr auto Wing = "#4D6E8C";
    constexpr auto Fin  = "#7A9CB8";

    // Fuselage
    Render::Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Cone    ({1.0f,  0, 0}, {1.6f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Sphere  ({-1.5f, 0, 0},               0.15f, Render::Color::Hex(Body), 12);

    // Main wings
    constexpr float span = 2.2f;
    Render::Triangle({-0.1f, -span, 0}, {-0.1f, span, 0}, { 0.5f, 0, 0}, Render::Color::Hex(Wing), true);
    Render::Triangle({-0.1f, -span, 0}, {-0.5f, 0,    0}, {-0.1f, span, 0}, Render::Color::Hex(Wing), true);

    // Horizontal stabilizers
    Render::Triangle({-1.3f, -0.6f, 0}, {-1.3f, 0.6f, 0}, {-0.9f, 0, 0}, Render::Color::Hex(Wing), true);

    // Vertical fin
    Render::Triangle({-1.4f, 0, 0}, {-1.0f, 0, 0}, {-1.25f, 0, -0.5f}, Render::Color::Hex(Fin), true);
}

// ── controls ────────────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");
    ImGui::Text("Lat %.6f  Lon %.6f  Alt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);
    ImGui::DragFloat("Speed", &aircraft_.speed, 0.5f, 0.f, 200.f, "%.1f m/s");
    ImGui::SliderFloat("Yaw",   &aircraft_.yaw,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &aircraft_.pitch, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::Separator();
    ImGui::Text("Camera: %s  [C]", cameraMode_.free ? "FreeCam" : "Chase");
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
    aircraft_.pitch = std::clamp(aircraft_.pitch, -80.f, 80.f);

    // ECEF-based velocity integration (works at all latitudes)
    double yr = glm::radians(double(aircraft_.yaw));
    double pr = glm::radians(double(aircraft_.pitch));
    double cp = std::cos(pr);

    double dNorth = aircraft_.speed * std::cos(yr) * cp * dt;
    double dEast  = aircraft_.speed * std::sin(yr) * cp * dt;
    double dUp    = aircraft_.speed * std::sin(pr) * dt;

    // Current ECEF position
    auto ecef = Render::GeoRef::ToEcef(aircraft_.lat, aircraft_.lon, aircraft_.alt);

    // Local NED unit vectors in ECEF at current position
    double phi = glm::radians(aircraft_.lat), lam = glm::radians(aircraft_.lon);
    double sphi = std::sin(phi), cphi = std::cos(phi);
    double slam = std::sin(lam), clam = std::cos(lam);

    glm::dvec3 north{-sphi * clam, -sphi * slam, cphi};
    glm::dvec3 east {-slam,         clam,         0.0};
    glm::dvec3 up   { cphi * clam,  cphi * slam,  sphi};

    // Integrate in ECEF
    ecef += north * dNorth + east * dEast + up * dUp;

    // ECEF → geodetic (Bowring iterative)
    double p = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y);
    aircraft_.lon = glm::degrees(std::atan2(ecef.y, ecef.x));
    double latRad = std::atan2(ecef.z, p * (1.0 - Render::GeoRef::e2));
    for (int i = 0; i < 3; ++i) {
        double sl = std::sin(latRad);
        double n  = Render::GeoRef::a / std::sqrt(1.0 - Render::GeoRef::e2 * sl * sl);
        latRad = std::atan2(ecef.z + Render::GeoRef::e2 * n * sl, p);
    }
    aircraft_.lat = glm::degrees(latRad);

    // Altitude (stable formula for all latitudes)
    double sl = std::sin(latRad), cl = std::cos(latRad);
    double n  = Render::GeoRef::a / std::sqrt(1.0 - Render::GeoRef::e2 * sl * sl);
    aircraft_.alt = (std::abs(cl) > 0.1)
        ? std::max(p / cl - n, 1.0)                             // equatorial formula
        : std::max(ecef.z / sl - n * (1.0 - Render::GeoRef::e2), 1.0);  // polar formula

    // Normalize longitude
    if (aircraft_.lon > 180.0)  aircraft_.lon -= 360.0;
    if (aircraft_.lon < -180.0) aircraft_.lon += 360.0;

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

        // Aircraft
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

        // Ground track
        Render::Cross({pos.x, pos.y, 0}, 0.3f, Render::Color::Hex("#FFFFFF30"), 1.5f);
        Render::Line(pos, {pos.x, pos.y, 0}, Render::Color::Hex("#FFFFFF15"), 1.f);

        // Trail (geodetic → local NED)
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
    if (focused && !cameraMode_.free) UpdatePhysics(dt);

    // Origin = aircraft → nedPos ≈ (0, 0, -alt) in NED
    SetupEnv("flight");
    auto nedPos = glm::vec3(Render::GeoToLocal("flight", aircraft_.lat, aircraft_.lon, aircraft_.alt));

    // Chase camera
    auto& flightCam = Render::GetCamera("flight");
    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.Follow(nedPos, aircraft_.yaw);
    else
        flightCam.Unfollow();

    DrawWorld("flight", nedPos);

    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.CaptureFollow();

    // Trail in geodetic
    Render::GeoCoord gc{aircraft_.lat, aircraft_.lon, aircraft_.alt};
    if (trail_.empty() || std::abs(gc.lat - trail_.back().lat) > 1e-7
                       || std::abs(gc.lon - trail_.back().lon) > 1e-7) {
        trail_.push_back(gc);
        if (trail_.size() > kTrailMax) trail_.erase(trail_.begin());
    }

    // Gimbal — overhead view looking at aircraft
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");
        auto gimbalPos = glm::vec3(Render::GeoToLocal("gimbal", aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto& gimbalCam = Render::GetCamera("gimbal");
        glm::vec3 gimbalEye = gimbalPos + glm::vec3(0.f, 0.f, 2.f);  // 2m above aircraft (NED: -Z = up)
        gimbalCam.LookAt(gimbalEye, gimbalPos);
        gimbalCam.Fov() = 60.f;
        DrawWorld("gimbal", gimbalPos);
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

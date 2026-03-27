#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include <imgui.h>
#include <cmath>

namespace Kilo {

Airspace::Airspace() : Panel("Airspace", "Airspace") {}

// ── aircraft model ──────────────────────────────────────────────

static void DrawAircraft() {
    constexpr auto Body = "#344b61";
    constexpr auto Wing = "#4D6E8C";
    constexpr auto Fin  = "#7A9CB8";

    // Fuselage
    Render::Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Cone    ({1.0f,  0, 0}, {1.6f, 0, 0}, 0.15f, Render::Color::Hex(Body), 12);
    Render::Sphere  ({-1.5f, 0, 0},               0.15f, Render::Color::Hex(Body), 12);

    // Main wings (two-sided sweep)
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
    ImGui::Text("Camera: %s  [C]", cameraMode_.free ? "FreeCam" : "Chase");
    if (!cameraMode_.free) ImGui::Checkbox("Chase Camera", &cameraMode_.chase);
    ImGui::Separator();
    ImGui::DragFloat ("Speed", &aircraft_.speed, 0.5f, 0.f, 50.f,   "%.1f u/s");
    ImGui::SliderFloat("Roll",  &aircraft_.roll,  -90.f,  90.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &aircraft_.pitch, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Yaw",   &aircraft_.yaw,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::Separator();
    ImGui::Text("N %.1f  E %.1f  Alt %.1f",
        aircraft_.position.x, aircraft_.position.y, -aircraft_.position.z);
    ImGui::TextDisabled(cameraMode_.free ? "RMB+WASD = fly camera" : "WASD = fly  |  MMB = orbit");
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

void Airspace::UpdatePhysics(float dt, bool focused) {
    aircraft_.pitch = std::clamp(aircraft_.pitch, -80.f, 80.f);

    // Bank autopilot: roll into turns
    float bank = 0.f;
    if (focused && !cameraMode_.free) {
        if (ImGui::IsKeyDown(ImGuiKey_D)) bank += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_A)) bank -= 1.f;
    }
    aircraft_.roll += (bank * 35.f - aircraft_.roll) * std::min(1.f, 5.f * dt);

    // Velocity integration (NED body → world)
    const float yr = glm::radians(aircraft_.yaw), pr = glm::radians(aircraft_.pitch);
    const float cp = std::cos(pr);
    aircraft_.position.x += aircraft_.speed * std::cos(yr) * cp * dt;
    aircraft_.position.y += aircraft_.speed * std::sin(yr) * cp * dt;
    aircraft_.position.z -= aircraft_.speed * std::sin(pr) * dt;
    aircraft_.position.z  = std::min(aircraft_.position.z, -0.1f);

    // Record trail by distance (adapts to speed: dense in turns, sparse on straights)
    if (trail_.empty() || glm::distance(aircraft_.position, trail_.back()) > 0.25f) {
        trail_.push_back(aircraft_.position);
        if (trail_.size() > kTrailMax) trail_.erase(trail_.begin());
    }
}

// ── scene ───────────────────────────────────────────────────────

void Airspace::DrawWorld(const char* scene) {
    Render::Begin(scene);
        Render::SetFrame(Render::FrameId::NED);
        Render::Grid();
        Render::Frame(glm::mat4(1.f), 1.f);

        // Aircraft
        Render::PushMatrix();
            Render::Translate(aircraft_.position);
            Render::RotateZ(aircraft_.yaw);
            Render::RotateY(aircraft_.pitch);
            Render::RotateX(aircraft_.roll);
            Render::PushMatrix();
                Render::Scale(0.4f);
                DrawAircraft();
                Render::PopMatrix();
            Render::Frame(glm::mat4(1.f), 0.5f);
            if (aircraft_.speed > 0.1f) {
                Render::Line({0, 0, 0}, {-aircraft_.speed * 0.06f, 0, 0}, Render::Color::Hex("#FFD700"), 5.f);
            }
            Render::Text({0, 0, 0}, Render::Color::Hex("#f8ffd8"), "(%d, %d, %d)",
                 (int)aircraft_.position.x, (int)aircraft_.position.y, (int)-aircraft_.position.z);
        Render::PopMatrix();

        // Ground track
        Render::Cross({aircraft_.position.x, aircraft_.position.y, 0}, 0.3f, Render::Color::Hex("#FFFFFF30"), 1.5f);
        Render::Line (aircraft_.position, {aircraft_.position.x, aircraft_.position.y, 0}, Render::Color::Hex("#FFFFFF15"), 1.f);

        // Trail
        if (trail_.size() > 1)
            Render::Trail(trail_.data(), static_cast<int>(trail_.size()), Render::Color::Hex("#FFD700"), 2.f);

        // Cardinal directions
        constexpr float d = 12.f;
        Render::Text({ d, 0, 0.1f}, Render::Color::Hex("#E07070"), "N");
        Render::Text({-d, 0, 0.1f}, Render::Color::Hex("#E07070"), "S");
        Render::Text({0,  d, 0.1f}, Render::Color::Hex("#70B870"), "E");
        Render::Text({0, -d, 0.1f}, Render::Color::Hex("#70B870"), "W");
    Render::End();
}

void Airspace::SetupEnv(const char* scene) {
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
    UpdatePhysics(dt, focused);

    // Main view — chase camera
    SetupEnv("flight");
    auto& flightCam = Render::GetCamera("flight");
    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.Follow(aircraft_.position, -aircraft_.yaw - 90.f);
    else
        flightCam.Unfollow();

    DrawWorld("flight");

    if (!cameraMode_.free && cameraMode_.chase)
        flightCam.CaptureFollow();

    // Camera position (read after DrawWorld so it reflects current frame)
    auto camPos = flightCam.Position();
    ImGui::Begin("Airspace");
    ImGui::TextDisabled("Cam  N %.1f  E %.1f  Alt %.1f", camPos.x, camPos.y, -camPos.z);
    ImGui::End();

    // Gimbal — mounted under aircraft, looking at origin
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");
        auto& gimbalCam = Render::GetCamera("gimbal");
        gimbalCam.LookAt(aircraft_.position + glm::vec3(0.f, 0.f, 0.3f), {0.f, 0.f, 0.f});
        gimbalCam.Fov() = 50.f;
        DrawWorld("gimbal");
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

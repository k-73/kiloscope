#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include <imgui.h>
#include <cmath>

namespace Kilo {

using namespace Render;
using namespace Render::Color;

Airspace::Airspace() : Panel("Airspace", "Airspace") {}

// ── aircraft model ──────────────────────────────────────────────

static void DrawAircraft() {
    constexpr auto Body = "#344b61";
    constexpr auto Wing = "#4D6E8C";
    constexpr auto Fin  = "#7A9CB8";

    // Fuselage
    Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Hex(Body), 12);
    Cone    ({1.0f,  0, 0}, {1.6f, 0, 0}, 0.15f, Hex(Body), 12);
    Sphere  ({-1.5f, 0, 0},               0.15f, Hex(Body), 12);

    // Main wings (two-sided sweep)
    constexpr float span = 2.2f;
    Triangle({-0.1f, -span, 0}, {-0.1f, span, 0}, { 0.5f, 0, 0}, Hex(Wing), true);
    Triangle({-0.1f, -span, 0}, {-0.5f, 0,    0}, {-0.1f, span, 0}, Hex(Wing), true);

    // Horizontal stabilizers
    Triangle({-1.3f, -0.6f, 0}, {-1.3f, 0.6f, 0}, {-0.9f, 0, 0}, Hex(Wing), true);

    // Vertical fin
    Triangle({-1.4f, 0, 0}, {-1.0f, 0, 0}, {-1.25f, 0, -0.5f}, Hex(Fin), true);
}

// ── controls ────────────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");
    ImGui::Text("Camera: %s  [C]", freecam_ ? "FreeCam" : "Chase");
    if (!freecam_) ImGui::Checkbox("Chase Camera", &chase_);
    ImGui::Separator();
    ImGui::DragFloat ("Speed", &speed_, 0.5f, 0.f, 50.f,   "%.1f u/s");
    ImGui::SliderFloat("Roll",  &roll_,  -90.f,  90.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &pitch_, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Yaw",   &yaw_,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::Separator();
    ImGui::Text("N %.1f  E %.1f  Alt %.1f", pos_.x, pos_.y, -pos_.z);
    ImGui::TextDisabled(freecam_ ? "RMB+WASD = fly camera" : "WASD = fly  |  MMB = orbit");
    ImGui::End();
}

// ── input ───────────────────────────────────────────────────────

void Airspace::HandleInput(float dt, bool focused) {
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        freecam_ = !freecam_;
        if (!freecam_) GetCamera("flight").ResetFollow();
    }
    if (!focused || freecam_) return;

    constexpr float kPitchRate = 40.f, kYawRate = 50.f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) pitch_ -= kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_S)) pitch_ += kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_A)) yaw_   -= kYawRate   * dt;
    if (ImGui::IsKeyDown(ImGuiKey_D)) yaw_   += kYawRate   * dt;
}

// ── physics ─────────────────────────────────────────────────────

void Airspace::UpdatePhysics(float dt, bool focused) {
    pitch_ = std::clamp(pitch_, -80.f, 80.f);

    // Bank autopilot: roll into turns
    float bank = 0.f;
    if (focused && !freecam_) {
        if (ImGui::IsKeyDown(ImGuiKey_D)) bank += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_A)) bank -= 1.f;
    }
    roll_ += (bank * 35.f - roll_) * std::min(1.f, 5.f * dt);

    // Velocity integration (NED body → world)
    const float yr = glm::radians(yaw_), pr = glm::radians(pitch_);
    const float cp = std::cos(pr);
    pos_.x += speed_ * std::cos(yr) * cp * dt;
    pos_.y += speed_ * std::sin(yr) * cp * dt;
    pos_.z -= speed_ * std::sin(pr) * dt;
    pos_.z  = std::min(pos_.z, -0.1f);
}

// ── scene ───────────────────────────────────────────────────────

void Airspace::DrawScene() {
    Begin("flight", {.frame = FrameId::NED});
        Grid();
        Frame(glm::mat4(1.f), 1.f);

        // Aircraft
        PushMatrix();
            Translate(pos_); RotateZ(yaw_); RotateY(pitch_); RotateX(roll_);
            PushMatrix(); Scale(0.4f); DrawAircraft(); PopMatrix();
            Frame(glm::mat4(1.f), 0.5f);
            if (speed_ > 0.1f)
                Line({0, 0, 0}, {-speed_ * 0.06f, 0, 0}, Hex("#FFD700"), 5.f);
            Text({0, 0, 0}, Hex("#f8ffd8"), "(%d, %d, %d)",
                 (int)pos_.x, (int)pos_.y, (int)-pos_.z);
        PopMatrix();

        // Ground track
        Cross(glm::vec3{pos_.x, pos_.y, 0}, 0.3f, Hex("#FFFFFF30"), 1.5f);
        Line (pos_, glm::vec3{pos_.x, pos_.y, 0}, Hex("#FFFFFF15"), 1.f);

        // Cardinal directions
        constexpr float d = 12.f;
        Text({ d, 0, 0.1f}, Hex("#E07070"), "N");
        Text({-d, 0, 0.1f}, Hex("#E07070"), "S");
        Text({0,  d, 0.1f}, Hex("#70B870"), "E");
        Text({0, -d, 0.1f}, Hex("#70B870"), "W");
    End();
}

// ── orchestrator ────────────────────────────────────────────────

void Airspace::OnDraw() {
    const float dt = ImGui::GetIO().DeltaTime;

    auto& env    = GetEnvironment("flight");
    env.bgColor  = {0.06f, 0.08f, 0.14f};
    env.showSun  = true;
    env.lightDir = NED::M * glm::vec3{0.4f, 0.2f, -0.8f};

    DrawControls();

    // Panel::Draw() wraps OnDraw() in ImGui::Begin(id_), so the current
    // window is the panel itself — which contains the 3D viewport.
    // Clicking the viewport focuses this window, enabling keyboard input.
    bool focused = ImGui::IsWindowFocused();
    HandleInput(dt, focused);
    UpdatePhysics(dt, focused);

    if (!freecam_ && chase_)
        GetCamera("flight").Follow(NED::M * pos_, -yaw_ - 90.f);
    else
        GetCamera("flight").Unfollow();

    DrawScene();

    if (!freecam_ && chase_)
        GetCamera("flight").CaptureFollow();
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo

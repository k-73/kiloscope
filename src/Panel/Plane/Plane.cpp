#include "Plane.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include <imgui.h>
#include <cmath>

namespace Kilo {

using namespace Render;
using namespace Render::Color;

PlanePanel::PlanePanel() : Panel("Plane", "Plane") {}

// ── static scene helpers ──────────────────────────────────────────

static void DrawAircraft() {
    constexpr auto kBody   = "#344b61ff";
    constexpr auto kWing   = "#4D6E8C";
    constexpr auto kFin    = "#7A9CB8";

    Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Hex(kBody), 12);
    Cone    ({1.0f,  0, 0}, {1.6f, 0, 0}, 0.15f, Hex(kBody), 12);
    Sphere  ({-1.5f, 0, 0},               0.15f, Hex(kBody), 12);

    constexpr float ws = 2.2f;
    Triangle({-0.1f, -ws,   0}, {-0.1f,  ws,   0}, { 0.5f,    0,    0}, Hex(kWing), true);
    Triangle({-0.1f, -ws,   0}, {-0.5f,   0,   0}, {-0.1f,   ws,    0}, Hex(kWing), true);
    Triangle({-1.3f, -0.6f, 0}, {-1.3f,  0.6f, 0}, {-0.9f,    0,    0}, Hex(kWing), true);
    Triangle({-1.4f,  0,    0}, {-1.0f,   0,   0}, {-1.25f,   0, -0.5f}, Hex(kFin),  true);
}

// ── controls window ──────────────────────────────────────────────

void PlanePanel::DrawControlsWindow() {
    ImGui::Begin("Aircraft State");
    ImGui::Text("Camera: %s  [C = toggle]", freecam_ ? "FreeCam" : "Chase");
    if (!freecam_) ImGui::Checkbox("Chase Camera", &chase_);
    ImGui::Separator();
    ImGui::DragFloat ("Speed", &speed_, 0.5f, 0.f, 50.f,   "%.1f u/s");
    ImGui::SliderFloat("Roll",  &roll_,  -90.f,  90.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &pitch_, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Yaw",   &yaw_,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::Separator();
    ImGui::Text("Pos: %.1f N  %.1f E  %.1f alt", pos_.x, pos_.y, -pos_.z);
    ImGui::TextDisabled(freecam_ ? "RMB+WASD = free camera"
                                 : "WASD = fly  |  MMB drag = camera angle");
    ImGui::End();
}

// ── input ────────────────────────────────────────────────────────

void PlanePanel::HandleInput(float dt, bool focused) {
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        freecam_ = !freecam_;
        if (!freecam_) { chaseYawOff_ = 0.f; chasePitchOff_ = 0.f; }
    }
    if (!focused || freecam_) return;

    constexpr float pitchRate = 40.f, yawRate = 50.f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) pitch_ -= pitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_S)) pitch_ += pitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_A)) yaw_   -= yawRate   * dt;
    if (ImGui::IsKeyDown(ImGuiKey_D)) yaw_   += yawRate   * dt;
}

// ── physics ──────────────────────────────────────────────────────

void PlanePanel::UpdatePhysics(float dt, bool focused) {
    pitch_ = std::clamp(pitch_, -80.f, 80.f);

    float bank = 0.f;
    if (focused && !freecam_) {
        if (ImGui::IsKeyDown(ImGuiKey_D)) bank += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_A)) bank -= 1.f;
    }
    roll_ += (bank * 35.f - roll_) * std::min(1.f, 5.f * dt);

    const float yr = glm::radians(yaw_), pr = glm::radians(pitch_);
    const float cp = std::cos(pr);
    pos_.x += speed_ * std::cos(yr) * cp * dt;
    pos_.y += speed_ * std::sin(yr) * cp * dt;
    pos_.z -= speed_ * std::sin(pr) * dt;
    pos_.z  = std::min(pos_.z, -0.1f);  // stay above ground
}

// ── camera ───────────────────────────────────────────────────────

void PlanePanel::UpdateChaseCamera() {
    if (freecam_ || !chase_) return;
    auto& cam      = GetCamera("flight");
    cam.Target()   = NED::M * pos_;
    cam.Yaw()      = -yaw_ - 90.f + chaseYawOff_;
    cam.Pitch()    = 18.f + chasePitchOff_;
    cam.Distance() = chaseDist_;
    camYawPrev_    = cam.Yaw();
    camPitchPrev_  = cam.Pitch();
}

// Draw.cpp applies MMB orbit and scroll zoom to the camera during Render::Begin/End.
// We capture those deltas and fold them into persistent offsets for the next frame.
void PlanePanel::CaptureChaseCamera() {
    if (freecam_ || !chase_) return;
    auto& cam      = GetCamera("flight");
    chaseYawOff_  += cam.Yaw()   - camYawPrev_;
    chasePitchOff_ = std::clamp(chasePitchOff_ + cam.Pitch() - camPitchPrev_, -70.f, 70.f);
    chaseDist_     = cam.Distance();
}

// ── scene ────────────────────────────────────────────────────────

void PlanePanel::DrawScene() {
    Begin("flight", {.frame = FrameId::NED});
        Grid();
        Frame(glm::mat4(1.f), 1.f);

        PushMatrix();
            Translate(pos_); RotateZ(yaw_); RotateY(pitch_); RotateX(roll_);
            PushMatrix(); Scale(0.4f); DrawAircraft(); PopMatrix();
            Frame(glm::mat4(1.f), 0.5f);
            if (speed_ > 0.1f) {
                float vl = -speed_ * 0.06f;
                Line({0, 0, 0}, {vl, 0, 0}, Hex("#FFD700"), 5.f);
            }
            Text({0, 0, 0}, Hex("#f8ffd8ff"), "(%d, %d, %d)", (int)pos_.x, (int)pos_.y, (int)-pos_.z);
        PopMatrix();

        Cross(glm::vec3{pos_.x, pos_.y, 0}, 0.3f, Hex("#FFFFFF30"), 1.5f);
        Line (pos_, glm::vec3{pos_.x, pos_.y, 0}, Hex("#FFFFFF15"), 1.f);

        constexpr float cd = 12.f;
        Text({ cd,   0, 0.1f}, Hex("#E07070"), "N");
        Text({-cd,   0, 0.1f}, Hex("#E07070"), "S");
        Text({  0,  cd, 0.1f}, Hex("#70B870"), "E");
        Text({  0, -cd, 0.1f}, Hex("#70B870"), "W");
    End();
}

// ── orchestrator ─────────────────────────────────────────────────

void PlanePanel::OnDraw() {
    const float dt = ImGui::GetIO().DeltaTime;

    auto& env    = GetEnvironment("flight");
    env.bgColor  = {0.06f, 0.08f, 0.14f};
    env.showSun  = true;
    env.lightDir = NED::M * glm::vec3{0.4f, 0.2f, -0.8f};  // NED: north-east, upward

    DrawControlsWindow();

    ImGui::Begin("Flight View");
    const bool focused = ImGui::IsWindowFocused();
    HandleInput(dt, focused);
    UpdatePhysics(dt, focused);
    UpdateChaseCamera();
    DrawScene();
    CaptureChaseCamera();
    ImGui::End();
}

static const bool reg_ = RegisterPanel<PlanePanel>("Plane", "Plane");

} // namespace Kilo

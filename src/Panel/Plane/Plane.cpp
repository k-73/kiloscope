#include "Plane.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
#include <imgui.h>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Kilo {

using namespace Render;
using namespace Render::Color;

PlanePanel::PlanePanel() : Panel("Plane", "Plane") {}

void PlanePanel::OnLoop() {
    prevT_ = t_;
    t_ = std::chrono::duration<float>(Clock::now() - start_).count();
}

// ── aircraft model (unit scale, body: X=fwd, Y=right, Z=down) ─────

static void DrawAircraft() {
    Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Hex("#C8C8D0"), 12);
    Cone({1.0f, 0, 0}, {1.6f, 0, 0}, 0.15f, Hex("#A0A0B0"), 12);
    Sphere({-1.5f, 0, 0}, 0.15f, Hex("#C8C8D0"), 12);

    constexpr float ws = 2.2f;
    Triangle({-0.1f, -ws, 0}, {-0.1f, ws, 0}, {0.5f, 0, 0}, Hex("#4070C0"));
    Triangle({-0.1f, -ws, 0}, {-0.5f, 0, 0}, {-0.1f, ws, 0}, Hex("#3060A8"));
    Triangle({-1.3f, -0.6f, 0}, {-1.3f, 0.6f, 0}, {-0.9f, 0, 0}, Hex("#4070C0"));
    Triangle({-1.4f, 0, 0}, {-1.0f, 0, 0}, {-1.25f, 0, -0.5f}, Hex("#C04040"));
}

static void DrawNedAxes(const glm::vec3& at, float len) {
    Arrow(at, at + glm::vec3{len, 0, 0}, Hex("#E05555"), 0.015f, 0.05f);
    Arrow(at, at + glm::vec3{0, len, 0}, Hex("#55B855"), 0.015f, 0.05f);
    Arrow(at, at + glm::vec3{0, 0, len}, Hex("#5580E6"), 0.015f, 0.05f);
    Text(at + glm::vec3{len * 1.1f, 0, 0}, Hex("#E05555"), "N");
    Text(at + glm::vec3{0, len * 1.1f, 0}, Hex("#55B855"), "E");
    Text(at + glm::vec3{0, 0, len * 1.1f}, Hex("#5580E6"), "D");
}

void PlanePanel::OnDraw() {
    float dt = ImGui::GetIO().DeltaTime;

    // ── environment ──────────────────────────────────────────────
    auto& env = GetEnvironment("flight");
    env.bgColor = {0.06f, 0.08f, 0.14f};
    env.showSun = true;
    env.lightDir = {0.4f, 0.2f, -0.8f};

    // ── controls window ──────────────────────────────────────────
    ImGui::Begin("Aircraft State");
        ImGui::Checkbox("Chase Camera", &chase_);
        ImGui::Separator();
        ImGui::DragFloat("Speed", &speed_, 0.5f, 0.f, 50.f, "%.1f u/s");
        ImGui::SliderFloat("Roll",  &roll_,  -90.f, 90.f,  "%.1f\xc2\xb0");
        ImGui::SliderFloat("Pitch", &pitch_, -45.f, 45.f,  "%.1f\xc2\xb0");
        ImGui::SliderFloat("Yaw",   &yaw_,  -180.f, 180.f, "%.1f\xc2\xb0");
        ImGui::Separator();
        ImGui::Text("Pos: %.1f N  %.1f E  %.1f alt", pos_.x, pos_.y, -pos_.z);
        ImGui::Text("WASD to fly (focus Flight View)");
    ImGui::End();

    // ── flight viewport ──────────────────────────────────────────
    ImGui::Begin("Flight View");
    bool focused = ImGui::IsWindowFocused();

    // ── keyboard input (when window focused) ─────────────────────
    if (focused) {
        constexpr float pitchRate = 40.f, yawRate = 50.f; // deg/s
        if (ImGui::IsKeyDown(ImGuiKey_W)) pitch_ -= pitchRate * dt; // nose down
        if (ImGui::IsKeyDown(ImGuiKey_S)) pitch_ += pitchRate * dt; // nose up
        if (ImGui::IsKeyDown(ImGuiKey_A)) yaw_   -= yawRate * dt;   // left
        if (ImGui::IsKeyDown(ImGuiKey_D)) yaw_   += yawRate * dt;   // right
    }
    pitch_ = std::clamp(pitch_, -80.f, 80.f);

    // ── roll follows yaw (bank into turn) ────────────────────────
    float yawInput = 0.f;
    if (focused && ImGui::IsKeyDown(ImGuiKey_A)) yawInput -= 1.f;
    if (focused && ImGui::IsKeyDown(ImGuiKey_D)) yawInput += 1.f;
    float targetRoll = yawInput * 35.f;
    roll_ += (targetRoll - roll_) * std::min(1.f, 5.f * dt); // smooth

    // ── integrate position along heading ─────────────────────────
    float yr = glm::radians(yaw_), pr = glm::radians(pitch_);
    float cp = std::cos(pr);
    pos_.x += speed_ * std::cos(yr) * cp * dt;
    pos_.y += speed_ * std::sin(yr) * cp * dt;
    pos_.z -= speed_ * std::sin(pr) * dt;
    pos_.z = std::min(pos_.z, -0.1f); // stay above ground

    // ── chase camera (set before Render::Begin) ──────────────────
    if (chase_) {
        auto& cam = GetCamera("flight");
        cam.Target()   = NED::M * pos_;             // NED → internal
        cam.Yaw()      = yaw_ - 90.f;               // NED heading → camera yaw
        cam.Pitch()    = std::clamp(pitch_ + 15.f, -60.f, 80.f);
        cam.Distance() = 4.f;
    }

    // ── render ───────────────────────────────────────────────────
    Begin("flight", {.frame = FrameId::NED});
        Grid();

        // Ground
        Box({0, 0, 0.01f}, {80, 80, 0.02f}, Hex("#151F15"));

        // Origin NED axes
        DrawNedAxes({0, 0, 0}, 1.5f);

        // Aircraft
        PushMatrix();
            Translate(pos_);
            RotateZ(yaw_);
            RotateY(pitch_);
            RotateX(roll_);

            PushMatrix();
                Scale(0.4f);
                DrawAircraft();
            PopMatrix();

            Frame(glm::mat4(1.f), 0.5f);

            // Velocity vector
            if (speed_ > 0.1f) {
                float vl = speed_ * 0.06f;
                Line({0,0,0}, {vl, 0, 0}, Hex("#FFD700"), 3.f);
                Text({vl + 0.1f, 0, 0}, Hex("#FFD700"), "%.1f", speed_);
            }
        PopMatrix();

        // Shadow on ground (projected position)
        Cross({pos_.x, pos_.y, 0}, 0.3f, Hex("#FFFFFF30"), 1.5f);

        // Altitude line
        Line({pos_.x, pos_.y, pos_.z}, {pos_.x, pos_.y, 0}, Hex("#FFFFFF15"), 1.f);

        // Cardinal labels
        constexpr float cd = 12.f;
        Text({ cd, 0, 0.1f}, Hex("#E07070"), "N");
        Text({-cd, 0, 0.1f}, Hex("#E07070"), "S");
        Text({0,  cd, 0.1f}, Hex("#70B870"), "E");
        Text({0, -cd, 0.1f}, Hex("#70B870"), "W");
    End();
    ImGui::End();
}

static const bool reg_ = RegisterPanel<PlanePanel>("Plane", "Plane");

} // namespace Kilo

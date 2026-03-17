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

void PlanePanel::OnLoop() {
    t_ = std::chrono::duration<float>(Clock::now() - start_).count();
}

// ── aircraft geometry (unit scale, NED body: X=fwd, Y=right, Z=down) ──

static void DrawAircraft() {
    // Fuselage
    Cylinder({-1.5f, 0, 0}, {1.0f, 0, 0}, 0.15f, Hex("#C8C8D0"), 12);
    Cone({1.0f, 0, 0}, {1.6f, 0, 0}, 0.15f, Hex("#A0A0B0"), 12);
    Sphere({-1.5f, 0, 0}, 0.15f, Hex("#C8C8D0"), 12);

    // Wings (symmetric, slight sweep)
    constexpr float span = 2.2f;
    Triangle({-0.1f, -span, 0}, {-0.1f, span, 0}, {0.5f, 0, 0}, Hex("#4070C0"));
    Triangle({-0.1f, -span, 0}, {-0.5f, 0, 0}, {-0.1f, span, 0}, Hex("#3060A8"));

    // Horizontal stabilizer
    Triangle({-1.3f, -0.6f, 0}, {-1.3f, 0.6f, 0}, {-0.9f, 0, 0}, Hex("#4070C0"));

    // Vertical stabilizer (points up = -Z in NED)
    Triangle({-1.4f, 0, 0}, {-1.0f, 0, 0}, {-1.25f, 0, -0.5f}, Hex("#C04040"));
}

// ── NED axes with labels ───────────────────────────────────────────

static void DrawNedAxes(float len) {
    Arrow({0,0,0}, {len, 0, 0},  Hex("#E05555"), 0.015f, 0.05f);
    Arrow({0,0,0}, {0, len, 0},  Hex("#55B855"), 0.015f, 0.05f);
    Arrow({0,0,0}, {0, 0, len},  Hex("#5580E6"), 0.015f, 0.05f);
    Text({len * 1.1f, 0, 0},     Hex("#E05555"), "N (X)");
    Text({0, len * 1.1f, 0},     Hex("#55B855"), "E (Y)");
    Text({0, 0, len * 1.1f},     Hex("#5580E6"), "D (Z)");
}

void PlanePanel::OnDraw() {
    // ── environment (set before rendering) ───────────────────────
    auto& env = GetEnvironment("flight");
    env.bgColor = {0.08f, 0.10f, 0.16f};
    env.showSun = true;
    env.lightDir = {0.4f, 0.2f, -0.8f};

    // ── controls ─────────────────────────────────────────────────
    ImGui::Begin("Aircraft State");
        ImGui::SliderFloat("Roll",  &roll_,  -90.f, 90.f,   "%.1f\xc2\xb0");
        ImGui::SliderFloat("Pitch", &pitch_, -45.f, 45.f,   "%.1f\xc2\xb0");
        ImGui::SliderFloat("Yaw",   &yaw_,  -180.f, 180.f,  "%.1f\xc2\xb0");
        ImGui::Separator();
        ImGui::DragFloat("Speed (m/s)",  &speed_,   1.f, 0.f, 300.f);
        ImGui::DragFloat("Altitude (m)", &altitude_, 10.f, 0.f, 12000.f);
    ImGui::End();

    // ── 3D viewport (NED frame) ──────────────────────────────────
    ImGui::Begin("Flight View");
    Begin("flight", {.frame = FrameId::NED});
        Grid();
        DrawNedAxes(2.f);

        // Ground reference
        Box({0, 0, 0.02f}, {30, 30, 0.04f}, Hex("#1A2A1A"));

        // Aircraft: yaw → pitch → roll (ZYX intrinsic = aerospace convention)
        float alt = altitude_ * 0.002f; // scale for viewport
        PushMatrix();
            Translate(0, 0, -alt);
            RotateZ(yaw_);
            RotateY(pitch_);
            RotateX(roll_);

            PushMatrix();
                Scale(0.5f);
                DrawAircraft();
            PopMatrix();

            // Body axes
            Frame(glm::mat4(1.f), 0.6f);

            // Velocity vector along body X
            float v = speed_ * 0.008f;
            Line({0,0,0}, {v, 0, 0}, Hex("#FFD700"), 3.f);
            Text({v + 0.15f, 0, 0}, Hex("#FFD700"), "V %.0f", speed_);
        PopMatrix();

        // Horizon ring at aircraft altitude
        Circle({0, 0, -alt}, {0, 0, 1}, 5.f, Hex("#FFFFFF20"), 64, 1.5f);

        // Cardinal directions on ground
        constexpr float cd = 7.f;
        Text({ cd, 0, 0},  Hex("#E07070"), "N");
        Text({-cd, 0, 0},  Hex("#E07070"), "S");
        Text({0,  cd, 0},  Hex("#70B870"), "E");
        Text({0, -cd, 0},  Hex("#70B870"), "W");
    End();
    ImGui::End();
}

static const bool reg_ = RegisterPanel<PlanePanel>("Plane", "Plane");

} // namespace Kilo

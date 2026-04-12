#include "Aircraft.hpp"
#include "Render/Draw.hpp"
#include "Render/Geo.hpp"
#include "Render/Model.hpp"
#include <GeographicLib/Geocentric.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace Kilo {

namespace {
Render::ModelId sModel = Render::kInvalidModel;
}

Aircraft::Aircraft() {
    if (sModel == Render::kInvalidModel)
        sModel = Render::LoadModel(std::string(ASSETS_DIR) + "/models/Jet_Lowpoly.obj");
}

void Aircraft::HandleInput(float dt, bool active) {
    if (!active) { bank_ = 0.f; return; }  // releases bank when defocused or in freecam
    constexpr float kPitchRate = 40.f, kYawRate = 50.f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) pitch -= kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_S)) pitch += kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_A)) { yaw -= kYawRate * dt; bank_ = -1.f; }
    if (ImGui::IsKeyDown(ImGuiKey_D)) { yaw += kYawRate * dt; bank_ =  1.f; }
    if (!ImGui::IsKeyDown(ImGuiKey_A) && !ImGui::IsKeyDown(ImGuiKey_D)) bank_ = 0.f;
}

void Aircraft::UpdatePhysics(float dt) {
    pitch = std::clamp(pitch, -80.f, 80.f);

    // NED velocity components from Euler attitude
    double yr = glm::radians(double(yaw));
    double pr = glm::radians(double(pitch));
    double dN = speed * std::cos(yr) * std::cos(pr) * dt;
    double dE = speed * std::sin(yr) * std::cos(pr) * dt;
    double dU = speed * std::sin(pr) * dt;

    // Integrate in ECEF (correct at all latitudes including poles)
    auto ecef = Render::GeoRef::ToEcef(lat, lon, alt);
    double phi = glm::radians(lat), lam = glm::radians(lon);
    double sp = std::sin(phi), cp = std::cos(phi);
    double sl = std::sin(lam), cl = std::cos(lam);
    glm::dvec3 N{-sp * cl, -sp * sl,  cp};
    glm::dvec3 E{-sl,       cl,        0.0};
    glm::dvec3 U{ cp * cl,  cp * sl,   sp};
    ecef += N * dN + E * dE + U * dU;

    static const auto& earth = GeographicLib::Geocentric::WGS84();
    earth.Reverse(ecef.x, ecef.y, ecef.z, lat, lon, alt);
    alt = std::max(alt, 1.0);

    // Smooth roll toward ±35° based on bank input
    roll += (bank_ * 35.f - roll) * std::min(1.f, 5.f * dt);
}

void Aircraft::DrawAt(const glm::vec3& posNed) const {
    Render::PushMatrix();
        Render::Translate(posNed);
        Render::RotateZ(yaw);
        Render::RotateY(pitch);
        Render::RotateX(roll);

        Render::Group group;

        // OBJ → body frame: scale, offset, axis remap
        Render::PushMatrix();
            Render::Scale(0.33f);
            Render::Translate(0, 0, -1.0f);
            // OBJ (X=right, Y=up, Z=fwd) → Body (X=fwd, Y=right, Z=down)
            Render::Transform(glm::mat4(
                glm::vec4(0, -1, 0, 0),
                glm::vec4(0,  0,-1, 0),
                glm::vec4(1,  0, 0, 0),
                glm::vec4(0,  0, 0, 1)));
            Render::Model(sModel, Render::Color::Hex("#3a5570"));
        Render::PopMatrix();

        // Afterburner — gradient beam, intensity ∝ speed
        if (speed > 0.f) {
            constexpr glm::vec3 kEngL{-2.2f, -0.21f, 0.f};
            constexpr glm::vec3 kEngR{-2.2f,  0.21f, 0.f};
            float t   = std::min(speed / 120.f, 1.f);
            float len = 0.3f + t * 1.2f;
            for (auto& eng : {kEngL, kEngR})
                Render::Beam(eng, eng + glm::vec3(-len, 0, 0),
                    {1.f, .9f, .7f, .8f * t}, {.9f, .2f, .05f, .1f * t}, 0.12f, 2.5f, 4);
        }
    Render::PopMatrix();
}

void Aircraft::DrawControls() {
    ImGui::Text("Lat %.6f  Lon %.6f  Alt %.0f m", lat, lon, alt);
    ImGui::DragFloat("Speed", &speed, 0.5f, 0.f, 200.f, "%.1f m/s");
    ImGui::SliderFloat("Yaw",   &yaw,   -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &pitch,  -45.f,  45.f, "%.1f\xc2\xb0");
}

} // namespace Kilo

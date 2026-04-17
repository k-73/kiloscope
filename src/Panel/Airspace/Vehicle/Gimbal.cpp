#include "Gimbal.hpp"
#include "Aircraft.hpp"
#include "Panel/Airspace/Navigation/Terrain.hpp"
#include "Render/Draw.hpp"
#include "Render/Geo.hpp"
#include "Ui/IconsFontAwesome7.h"
#include "Ui/Widget.hpp"
#include <imgui.h>
#include <cmath>

namespace Kilo {

glm::vec3 Gimbal::PositionNed(const glm::vec3& aircraftNed) const {
    return aircraftNed + aircraft_.BodyToNed() * bodyOffset;
}

glm::vec3 Gimbal::TargetNed(const char* scene) const {
    return glm::vec3(Render::GeoToLocal(scene, targetLat, targetLon, targetAlt));
}

glm::vec3 Gimbal::TargetNed() const {
    return glm::vec3(Render::GeoToLocal(targetLat, targetLon, targetAlt));
}

void Gimbal::DrawFrustum(const glm::vec3& aircraftNed) const {
    auto bodyToNed = aircraft_.BodyToNed();
    auto gimbalPos = aircraftNed + bodyToNed * bodyOffset;
    auto targetPos = TargetNed();
    auto sightDir  = glm::normalize(targetPos - gimbalPos);
    auto upDir     = bodyToNed * glm::vec3(0, 0, -1);

    Render::Line(aircraftNed, gimbalPos, Render::Color::Hex("#d4985b50"), 1.f);
    Render::Sensor(gimbalPos, sightDir, upDir, fov, aspect, 0.1,
        Render::Color::Hex("#90B0D0"), 1.0f);
    Render::Line(gimbalPos, targetPos, Render::Color::Hex("#00c3ffAA"), 1.f);
}

void Gimbal::DrawTargetMarker() {
    Render::Group group;
    Render::Marker(TargetNed(), ICON_FA_CROSSHAIRS, "Target", Render::Color::Hex("#00ccffff"),
        "Lat %.6f\nLon %.6f\nAlt %.0f m", targetLat, targetLon, targetAlt);

    if (Render::Event().Dragging()) {
        auto& io = ImGui::GetIO();
        double lat, lon, alt;
        if (terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt, "flight"))
            SetTarget(lat, lon, alt);
    }
}

void Gimbal::ApplyJoystickInput(glm::vec2 joy, float dt) {
    // Quadratic response — soft at center, precise aim at full deflection.
    constexpr float kRate = 0.0002f;
    float panRight   =  joy.x * std::abs(joy.x) * kRate * dt;
    float panForward = -joy.y * std::abs(joy.y) * kRate * dt;

    // Rotate body-relative pan (forward/right) into NED lat/lon deltas.
    float yawRad = glm::radians(aircraft_.yaw);
    float cosLat = std::cos(glm::radians(float(aircraft_.lat)));
    targetLat += panForward * std::cos(yawRad) - panRight * std::sin(yawRad);
    targetLon += (panForward * std::sin(yawRad) + panRight * std::cos(yawRad))
                 / std::max(cosLat, 0.01f);
    targetAlt  = double(terrain_.Sample(targetLat, targetLon));
}

void Gimbal::DrawControls() {
    ImGui::SliderFloat("FOV",    &fov,    5.f, 120.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Aspect", &aspect, 0.5f, 3.f,   "%.2f");

    bool changed = false;
    changed |= ImGui::InputDouble("Target Lat", &targetLat, 0.01, 0.1, "%.6f");
    changed |= ImGui::InputDouble("Target Lon", &targetLon, 0.01, 0.1, "%.6f");
    if (changed) targetAlt = double(terrain_.Sample(targetLat, targetLon));

    ImGui::BeginDisabled();
    ImGui::InputDouble("Target Alt", &targetAlt, 0, 0, "%.0f m");
    ImGui::EndDisabled();

    glm::vec2 joy;
    if (Widget::Joystick("##gimbal", &joy))
        ApplyJoystickInput(joy, ImGui::GetIO().DeltaTime);
}

} // namespace Kilo

#include "Gimbal.hpp"
#include "Aircraft.hpp"
#include "Terrain.hpp"
#include "Render/Draw.hpp"
#include "Render/Geo.hpp"
#include "Ui/IconsFontAwesome7.h"
#include "Ui/Widget.hpp"
#include <imgui.h>
#include <cmath>

namespace Kilo {

glm::vec3 Gimbal::PositionFrom(const glm::vec3& aircraftNed, const Aircraft& aircraft) const {
    return aircraftNed + aircraft.BodyToNed() * bodyOffset;
}

glm::vec3 Gimbal::TargetInScene(const char* scene) const {
    return glm::vec3(Render::GeoToLocal(scene, targetLat, targetLon, targetAlt));
}

void Gimbal::ApplyJoystickInput(glm::vec2 joy, float dt,
                                const Aircraft& aircraft, const Terrain& terrain) {
    constexpr float kRate = 0.0002f;
    // Quadratic response — fine control near center, fast at edges
    float fx = joy.x * std::abs(joy.x) * kRate * dt;
    float fy = -joy.y * std::abs(joy.y) * kRate * dt;

    // Rotate stick axes to aircraft heading, then apply cos(lat) correction for longitude
    float yr     = glm::radians(aircraft.yaw);
    float cosLat = std::cos(glm::radians(float(aircraft.lat)));
    targetLat += fy * std::cos(yr) - fx * std::sin(yr);
    targetLon += (fy * std::sin(yr) + fx * std::cos(yr)) / std::max(cosLat, 0.01f);
    targetAlt  = double(terrain.Sample(targetLat, targetLon));
}

void Gimbal::DrawFrustum(const glm::vec3& aircraftNed, const Aircraft& aircraft) const {
    auto bodyToNed = aircraft.BodyToNed();
    auto gimbalNed = aircraftNed + bodyToNed * bodyOffset;
    auto targetNed = glm::vec3(Render::GeoToLocal(targetLat, targetLon, targetAlt));
    auto dir       = glm::normalize(targetNed - gimbalNed);
    auto up        = bodyToNed * glm::vec3(0, 0, -1);  // body up in NED
    Render::Line(aircraftNed, gimbalNed, Render::Color::Hex("#d4985b50"), 1.f);
    Render::Sensor(gimbalNed, dir, up, fov, aspect, 0.1,
        Render::Color::Hex("#90B0D0"), 1.0f);
    Render::Line(gimbalNed, targetNed, Render::Color::Hex("#00c3ffAA"), 1.f);
}

void Gimbal::DrawTargetMarker(const Terrain& terrain) {
    Render::Group g;
    auto targetNed = glm::vec3(Render::GeoToLocal(targetLat, targetLon, targetAlt));
    Render::Marker(targetNed, ICON_FA_CROSSHAIRS, "Target", Render::Color::Hex("#00ccffff"),
        "Lat %.6f\nLon %.6f\nAlt %.0f m", targetLat, targetLon, targetAlt);
    if (Render::Event().Dragging()) {
        auto& io = ImGui::GetIO();
        double lat, lon, alt;
        if (terrain.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt))
            SetTarget(lat, lon, alt);
    }
}

void Gimbal::DrawControls(const Aircraft& aircraft, const Terrain& terrain) {
    ImGui::SliderFloat("FOV",    &fov,    5.f, 120.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Aspect", &aspect, 0.5f, 3.f,   "%.2f");

    bool changed = false;
    changed |= ImGui::InputDouble("Target Lat", &targetLat, 0.01, 0.1, "%.6f");
    changed |= ImGui::InputDouble("Target Lon", &targetLon, 0.01, 0.1, "%.6f");
    if (changed) targetAlt = double(terrain.Sample(targetLat, targetLon));

    ImGui::BeginDisabled();
    ImGui::InputDouble("Target Alt", &targetAlt, 0, 0, "%.0f m");
    ImGui::EndDisabled();

    // Heading-relative joystick: forward = aircraft heading, cos(lat)-corrected lon
    glm::vec2 joy;
    if (Widget::Joystick("##gimbal", &joy))
        ApplyJoystickInput(joy, ImGui::GetIO().DeltaTime, aircraft, terrain);
}

} // namespace Kilo

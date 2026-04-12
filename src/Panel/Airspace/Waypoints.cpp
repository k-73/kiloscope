#include "Waypoints.hpp"
#include "Gimbal.hpp"
#include "Terrain.hpp"
#include "Render/Draw.hpp"
#include "Render/Geo.hpp"
#include "Ui/IconsFontAwesome7.h"
#include <imgui.h>

namespace Kilo {

void Waypoints::Add(double lat, double lon, double alt) {
    list.push_back({lat, lon, alt, "WP" + std::to_string(list.size() + 1)});
}

void Waypoints::SnapToTerrain() {
    for (auto& wp : list)
        wp.alt = double(terrain_.Sample(wp.lat, wp.lon));
}

bool Waypoints::Draw(const glm::vec3& aircraftPos) {
    bool targetOnWaypoint = false;
    for (auto& wp : list) {
        bool isTarget = (wp.lat == gimbal_.targetLat && wp.lon == gimbal_.targetLon);
        if (isTarget) targetOnWaypoint = true;

        auto wpLocal = glm::vec3(Render::GeoToLocal(wp.lat, wp.lon, wp.alt));
        float dist   = glm::length(wpLocal - aircraftPos) * 0.001f;
        const char* icon = isTarget ? ICON_FA_CROSSHAIRS : ICON_FA_LOCATION_DOT;
        auto color = isTarget ? Render::Color::Hex("#00ccffff") : wp.color;
        Render::Marker(wpLocal, icon, wp.label.c_str(), color,
            "%s%.2f km\n%.6f, %.6f\n%.0f m",
            isTarget ? "Target\n" : "", dist, wp.lat, wp.lon, wp.alt);

        auto ev = Render::Event();
        if (ev.Dragging()) {
            auto& io = ImGui::GetIO();
            double lat, lon, alt;
            if (terrain_.ScreenToSurface(io.MousePos.x, io.MousePos.y, lat, lon, alt)) {
                wp.lat = lat; wp.lon = lon; wp.alt = alt;
                if (isTarget) gimbal_.SetTarget(lat, lon, alt);
            }
        }
        if (ev.Clicked(Render::Right)) {
            gimbal_.SetTarget(wp.lat, wp.lon, wp.alt);
            rightOnMarker = true;
        }
    }
    return targetOnWaypoint;
}

void Waypoints::DrawControls() {
    int removeIdx = -1;
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID(int(i));
        auto& wp = list[i];
        ImGui::Text("%s", wp.label.c_str());
        ImGui::SameLine();
        ImGui::Text("%.6f, %.6f", wp.lat, wp.lon);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = int(i);
        ImGui::PopID();
    }
    if (removeIdx >= 0)
        list.erase(list.begin() + removeIdx);
    ImGui::TextDisabled("Double-click globe to add waypoint");
}

json Waypoints::Save() const {
    json arr = json::array();
    for (auto& wp : list) {
        arr.push_back({
            {"lat", wp.lat}, {"lon", wp.lon}, {"alt", wp.alt}, {"label", wp.label},
            {"color", {wp.color.r, wp.color.g, wp.color.b, wp.color.a}},
        });
    }
    return arr;
}

void Waypoints::Load(const json& j) {
    list.clear();
    for (auto& w : j) {
        Waypoint wp;
        wp.lat   = w.value("lat", 0.0);
        wp.lon   = w.value("lon", 0.0);
        wp.alt   = w.value("alt", 0.0);
        wp.label = w.value("label", std::string("WP"));
        if (w.contains("color") && w["color"].is_array() && w["color"].size() == 4)
            wp.color = {w["color"][0], w["color"][1], w["color"][2], w["color"][3]};
        list.push_back(std::move(wp));
    }
}

} // namespace Kilo

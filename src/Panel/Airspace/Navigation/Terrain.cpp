#include "Terrain.hpp"
#include "Panel/Airspace/Vehicle/Aircraft.hpp"
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <string>

namespace Kilo {

Terrain::Terrain(const Aircraft& aircraft) : aircraft_(aircraft) {
    future_ = std::async(std::launch::async, [] {
        return Render::LoadTerrainDir(std::string(ASSETS_DIR) + "/terrain",
                                      std::string(ASSETS_DIR) + "/geoid/egm2008-5.pgm");
    });
}

bool Terrain::Poll() {
    if (ready_ || !future_.valid()) return false;
    if (future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
    set_   = future_.get();
    ready_ = true;
    return true;
}

bool Terrain::ScreenToSurface(float sx, float sy, double& lat, double& lon, double& alt,
                              const char* scene) const {
    double h = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (!Render::ScreenToGeo(scene, sx, sy, lat, lon, alt, h)) return false;
        h = double(Sample(lat, lon));
    }
    alt = h;
    return true;
}

void Terrain::RebuildIfNeeded(bool force) {
    if (!ready_) return;
    double lat = aircraft_.lat, lon = aircraft_.lon;
    constexpr double kDegPerKm = 1.0 / 111.32;
    double cosLat    = std::cos(glm::radians(lat));
    double threshDeg = config.rebuildKm * kDegPerKm;
    double dLat      = lat - centerLat_;
    double dLon      = (lon - centerLon_) * cosLat;
    if (force || mesh_.indices.empty() || dLat * dLat + dLon * dLon > threshDeg * threshDeg) {
        centerLat_ = lat;
        centerLon_ = lon;
        float latRadDeg = float(config.radiusKm * kDegPerKm);
        float lonRadDeg = float(latRadDeg / std::max(cosLat, 0.01));
        float stepDeg   = float(config.resolutionM / 111320.0);
        mesh_ = Render::BuildTerrainMesh(set_, lat, lon, latRadDeg, lonRadDeg, stepDeg);
    }
}

void Terrain::DrawControls() {
    bool changed = false;
    changed |= ImGui::DragFloat("Radius",     &config.radiusKm,    0.5f,  1.f,  50.f, "%.1f km");
    changed |= ImGui::DragFloat("Resolution", &config.resolutionM, 5.f,  10.f, 500.f, "%.0f m");
    changed |= ImGui::DragFloat("Rebuild",    &config.rebuildKm,   0.5f,  1.f,  30.f, "%.1f km");
    if (changed) RebuildIfNeeded(true);
    ImGui::TextDisabled("%d verts, %d tris",
        int(mesh_.relPos.size()), mesh_.indexCount / 3);
}

json Terrain::Save() const {
    return {
        {"radiusKm",    config.radiusKm},
        {"resolutionM", config.resolutionM},
        {"rebuildKm",   config.rebuildKm},
    };
}

void Terrain::Load(const json& j) {
    config.radiusKm    = j.value("radiusKm",    config.radiusKm);
    config.resolutionM = j.value("resolutionM", config.resolutionM);
    config.rebuildKm   = j.value("rebuildKm",   config.rebuildKm);
}

} // namespace Kilo

#pragma once
#include "Render/Draw.hpp"
#include <future>
#include <nlohmann/json.hpp>

namespace Kilo {

using json = nlohmann::json;

// Terrain — owns multi-tile GeoTIFF set + local mesh, async loaded at construction.
// Mesh rebuilds when aircraft moves past `rebuildKm` from current center.
class Terrain {
public:
    struct Config {
        float radiusKm    = 15.f;  // mesh radius around aircraft
        float resolutionM = 50.f;  // vertex spacing
        float rebuildKm   = 5.f;   // rebuild trigger distance
    } config;

    Terrain();        // kicks off async load
    bool Poll();      // call from OnDraw; returns true once when terrain becomes ready

    bool  Ready()                        const { return ready_; }
    float Sample(double lat, double lon) const { return set_.Sample(lat, lon); }
    float ElevMin()                      const { return set_.elevMin; }
    float ElevMax()                      const { return set_.elevMax; }

    // Iterative ray-terrain intersection (3 passes, converges).
    bool ScreenToSurface(float sx, float sy, double& lat, double& lon, double& alt,
                         const char* scene = "flight") const;

    // Rebuild mesh if aircraft moved past threshold (or `force`).
    void RebuildIfNeeded(double aircraftLat, double aircraftLon, bool force = false);

    // Draw current mesh — call inside Begin/End scope.
    void Draw() { if (ready_) Render::DrawTerrain(mesh_, set_.elevMin, set_.elevMax); }

    void DrawControls(double aircraftLat, double aircraftLon);

    json Save() const;
    void Load(const json& j);

private:
    std::future<Render::TerrainSet> future_;
    Render::TerrainSet              set_;
    Render::TerrainMesh             mesh_;
    double                          centerLat_ = 0, centerLon_ = 0;
    bool                            ready_     = false;
};

} // namespace Kilo

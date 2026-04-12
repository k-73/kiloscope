#pragma once
#include "Render/Draw.hpp"
#include <future>
#include <nlohmann/json.hpp>

namespace Kilo {

using json = nlohmann::json;

class Aircraft;

// Terrain — owns multi-tile GeoTIFF set + local mesh, async loaded at construction.
// Mesh rebuilds when aircraft moves past `rebuildKm` from current center.
// Holds a const reference to Aircraft for position-dependent operations.
class Terrain {
public:
    struct Config {
        float radiusKm    = 15.f;
        float resolutionM = 50.f;
        float rebuildKm   = 5.f;
    } config;

    explicit Terrain(const Aircraft& aircraft);
    bool Poll();      // returns true once when terrain becomes ready

    bool  Ready()                        const { return ready_; }
    float Sample(double lat, double lon) const { return set_.Sample(lat, lon); }
    float ElevMin()                      const { return set_.elevMin; }
    float ElevMax()                      const { return set_.elevMax; }

    // Iterative ray-terrain intersection (3 passes, converges).
    bool ScreenToSurface(float sx, float sy, double& lat, double& lon, double& alt,
                         const char* scene) const;

    // Rebuild mesh if aircraft moved past threshold (or `force`).
    void RebuildIfNeeded(bool force = false);

    // Draw current mesh — call inside Begin/End scope.
    void Draw() { if (ready_) Render::DrawTerrain(mesh_, set_.elevMin, set_.elevMax); }

    void DrawControls();

    json Save() const;
    void Load(const json& j);

private:
    const Aircraft&                 aircraft_;
    std::future<Render::TerrainSet> future_;
    Render::TerrainSet              set_;
    Render::TerrainMesh             mesh_;
    double                          centerLat_ = 0, centerLon_ = 0;
    bool                            ready_     = false;
};

} // namespace Kilo

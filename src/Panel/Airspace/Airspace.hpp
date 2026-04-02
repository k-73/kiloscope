#pragma once
#include "Core/Panel/Panel.hpp"
#include "Render/Draw.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include "Render/Trail.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Kilo {

// Geodetic waypoint — lat/lon/alt with label and color.
struct Waypoint {
    double    lat = 0.0, lon = 0.0, alt = 0.0;
    std::string label;
    glm::vec4 color = {1.f, .6f, .2f, 1.f};  // orange default
};

class Airspace : public Panel {
public:
    Airspace();
    void OnLoop() override;
    void OnDraw() override;

    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    void DrawControls();
    void HandleInput(float dt, bool focused);
    void UpdatePhysics(float dt);
    glm::mat3 BodyToNed() const { return Render::EulerZYX(aircraft_.yaw, aircraft_.pitch, aircraft_.roll); }
    void DrawWorld(const glm::vec3& pos);
    void DrawFlight(float dt);
    void DrawGimbal();
    void SetupEnv(const char* scene);

    // Aircraft (geodetic absolute)
    struct AircraftState {
        double lat = 52.2297, lon = 21.0122, alt = 20.0;
        float  yaw = 0.f, pitch = 0.f, roll = 0.f, speed = 0.f;
    } aircraft_;

    // Gimbal (mounted under aircraft)
    struct GimbalState {
        glm::vec3 bodyOffset = {0.0f, 0.f, 0.5f};  // body frame (X=fwd, Y=right, Z=down)
        float     fov        = 50.f;                 // vertical FOV (degrees)
        float     aspect     = 16.f / 9.f;           // width / height
        double    targetLat  = 52.2297, targetLon = 21.0122, targetAlt = 0.0;
    } gimbal_;

    bool cameraFree_ = false;
    float bank_      = 0.f;   // bank input from HandleInput (focus-guarded)

    std::vector<Waypoint> waypoints_;
    Render::TrailBuffer trail_{128, 1.0};
    Render::TerrainSet terrain_;
    Render::TerrainMesh terrainMesh_;
    double terrainCenterLat_ = 0.0, terrainCenterLon_ = 0.0;

    struct TerrainCfg {
        float radiusKm = 15.f;    // mesh radius around aircraft
        float resolutionM = 50.f; // vertex spacing in meters
        float rebuildKm = 5.f;    // rebuild when aircraft moves this far
    } terrainCfg_;

    void RebuildTerrainIfNeeded(bool force = false);
    bool ScreenToTerrain(float sx, float sy, double& lat, double& lon, double& alt,
                         const char* scene = "flight");
};

} // namespace Kilo

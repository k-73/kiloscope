#pragma once
#include "Vehicle/Aircraft.hpp"
#include "Vehicle/Gimbal.hpp"
#include "Navigation/Terrain.hpp"
#include "Navigation/Waypoints.hpp"
#include "Core/Panel/Panel.hpp"
#include "Render/Trail.hpp"

namespace Kilo {

// Airspace — interactive flight visualization with terrain, waypoints, and gimbal POV.
class Airspace : public Panel {
public:
    Airspace();

    void OnLoop() override;
    void OnDraw() override;

    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    // Lifecycle
    void OnTerrainReady();

    // Flight view pipeline
    void DrawFlightView(float dt);
    void UpdateFlightCamera(const glm::vec3& aircraftNed, float dt);
    void DrawFlightScene(const glm::vec3& aircraftNed);
    void SyncWaypoints(const glm::vec3& aircraftNed);
    void DrawAircraftIndicators(const glm::vec3& aircraftNed);
    void DrawFlightHud();
    void HandleFlightMouse();

    // Gimbal view pipeline
    void DrawGimbalView();
    void UpdateGimbalCamera(const glm::vec3& gimbalPos, const glm::vec3& targetPos);
    void DrawGimbalScene(const glm::vec3& aircraftNed);
    void DrawGimbalHud(const glm::vec3& gimbalPos, const glm::vec3& targetPos);

    // Shared
    void DrawWorld(const glm::vec3& aircraftNed);
    void DrawControls();
    void SetupEnv(const char* scene);

    // Declaration order matters: deps before dependents.
    Aircraft  aircraft_;
    Terrain   terrain_{aircraft_};
    Gimbal    gimbal_{aircraft_, terrain_};
    Waypoints waypoints_;
    Render::TrailBuffer trail_{128, 1.0};

    Waypoints::DrawResult waypointEvents_;  // flight-scene interactions, consumed same frame
    bool cameraFree_  = false;
    bool rmbOnMarker_ = false;              // sticky across frames until RMB released
};

} // namespace Kilo

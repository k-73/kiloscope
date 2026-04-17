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
    void DrawFlightView(float dt);
    void DrawGimbalView();
    void DrawWorld(const glm::vec3& aircraftNed);
    void HandleFlightMouse();
    void OnTerrainReady();
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

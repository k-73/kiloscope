#pragma once
#include "Aircraft.hpp"
#include "Gimbal.hpp"
#include "Terrain.hpp"
#include "Waypoints.hpp"
#include "Core/Panel/Panel.hpp"
#include "Render/Trail.hpp"

namespace Kilo {

// Airspace — interactive flight visualization with terrain, waypoints, and gimbal POV.
// Owns a small set of focused modules and orchestrates two scenes (flight + gimbal).
class Airspace : public Panel {
public:
    Airspace();

    void OnLoop() override;
    void OnDraw() override;

    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    void DrawControls();
    void DrawGlobeControls();
    void DrawFlightView(float dt);
    void DrawGimbalView();
    void DrawWorld(const glm::vec3& aircraftPosNed);
    void SetupEnv(const char* scene);

    Aircraft  aircraft_;
    Gimbal    gimbal_;
    Waypoints waypoints_;
    Terrain   terrain_;
    Render::TrailBuffer trail_{128, 1.0};

    // Per-frame interaction state (not persisted).
    bool cameraFree_       = false;  // toggled with C
    bool rightOnMarker_    = false;  // right-press started on a marker — suppress terrain handler
    bool targetOnWaypoint_ = false;  // target coincides with a waypoint — skip standalone marker
};

} // namespace Kilo

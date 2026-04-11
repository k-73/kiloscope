#pragma once
#include "Aircraft.hpp"
#include "Gimbal.hpp"
#include "Terrain.hpp"
#include "Waypoints.hpp"
#include "Core/Panel/Panel.hpp"
#include "Render/Trail.hpp"

namespace Kilo {

// Airspace — interactive flight visualization with terrain, waypoints, and gimbal POV.
// Orchestrates two scenes (flight + gimbal) over a shared world state.
class Airspace : public Panel {
public:
    Airspace();

    void OnLoop() override;
    void OnDraw() override;

    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    // views
    void DrawFlightView(float dt);
    void DrawFlightScene(const glm::vec3& aircraftNed);
    void HandleFlightMouse();

    void DrawGimbalView();
    void DrawGimbalScene(const glm::vec3& aircraftNed);

    // shared world content (drawn in both scenes)
    void DrawWorld(const glm::vec3& aircraftNed);

    // lifecycle & ui
    void OnTerrainReady();
    void DrawControls();
    void DrawGlobeControls();
    void SetupEnv(const char* scene);

    // state
    Aircraft  aircraft_;
    Gimbal    gimbal_;
    Waypoints waypoints_;
    Terrain   terrain_;
    Render::TrailBuffer trail_{128, 1.0};

    // Per-frame interaction flags (not persisted).
    bool cameraFree_       = false;  // toggled with C
    bool rightOnMarker_    = false;  // right-press started on a marker — suppress terrain handler
    bool targetOnWaypoint_ = false;  // target coincides with a waypoint — skip standalone marker
};

} // namespace Kilo

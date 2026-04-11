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
//
// Method organization enforces separation between state mutation and rendering:
//   Update*/Handle*  mutate state (camera math, input, physics sync)
//   Draw*            read state and emit render calls (no state mutation)
//   SaveSettings/LoadSettings  persistence
class Airspace : public Panel {
public:
    Airspace();

    void OnLoop() override;
    void OnDraw() override;

    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    // ── orchestration ─────────────────────────────────────
    void DrawFlightView(float dt);
    void DrawGimbalView();

    // ── state update (math, input, lifecycle) ────────────
    void OnTerrainReady();
    void UpdateFlightCamera(const glm::vec3& aircraftNed);
    void HandleFlightInput(float dt);
    void HandleFlightMouse();
    void UpdateGimbalCamera(const glm::vec3& gimbalNed, const glm::vec3& targetNed);

    // ── rendering (scene + overlays) ──────────────────────
    void DrawFlightScene(const glm::vec3& aircraftNed);
    void DrawFlightOverlays();
    void DrawGimbalScene(const glm::vec3& aircraftNed);
    void DrawGimbalOverlay(float distance);
    void DrawWorld(const glm::vec3& aircraftNed);

    // ── UI ────────────────────────────────────────────────
    void DrawControls();
    void DrawGlobeControls();

    // ── scene config ──────────────────────────────────────
    void SetupEnv(const char* scene);

    // ── state ─────────────────────────────────────────────
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

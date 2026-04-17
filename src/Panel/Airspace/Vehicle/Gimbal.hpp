#pragma once
#include <glm/glm.hpp>

namespace Kilo {

class Aircraft;
class Terrain;

// Gimbal — sensor mounted on aircraft body, looking at a geodetic target.
// State is geodetic only; NED positions are computed on demand (scene-aware).
class Gimbal {
public:
    Gimbal(const Aircraft& aircraft, const Terrain& terrain)
        : aircraft_(aircraft), terrain_(terrain) {}

    glm::vec3 bodyOffset = {0.0f, 0.f, 0.5f};  // body frame: X=fwd, Y=right, Z=down
    float     fov        = 50.f;
    float     aspect     = 16.f / 9.f;
    double    targetLat  = 52.2297, targetLon = 21.0122, targetAlt = 0.0;

    void SetTarget(double lat, double lon, double alt) {
        targetLat = lat; targetLon = lon; targetAlt = alt;
    }

    // Gimbal mount position in given scene's NED frame.
    glm::vec3 PositionNed(const glm::vec3& aircraftNed) const;

    // Target position — scene-aware (pass scene explicitly or call inside Begin/End).
    glm::vec3 TargetNed(const char* scene) const;
    glm::vec3 TargetNed() const;  // uses current scene

    // Renders frustum + line to target. Call inside Begin/End.
    void DrawFrustum(const glm::vec3& aircraftNed) const;

    // Renders crosshair at target. Call inside Begin/End; handles drag raycast on terrain.
    void DrawTargetMarker();

    void DrawControls();

private:
    void ApplyJoystickInput(glm::vec2 joy, float dt);

    const Aircraft& aircraft_;
    const Terrain&  terrain_;
};

} // namespace Kilo

#pragma once
#include <glm/glm.hpp>

namespace Kilo {

class Aircraft;
class Terrain;

// Gimbal — sensor mounted on aircraft body. Looks at a geodetic target.
// State is plain public fields; helpers compute scene-relative positions
// from a precomputed aircraft NED position (no redundant GeoToLocal).
class Gimbal {
public:
    glm::vec3 bodyOffset = {0.0f, 0.f, 0.5f};  // body frame: X=fwd, Y=right, Z=down
    float     fov        = 50.f;                // vertical FOV (degrees)
    float     aspect     = 16.f / 9.f;
    double    targetLat  = 52.2297, targetLon = 21.0122, targetAlt = 0.0;

    void SetTarget(double lat, double lon, double alt) {
        targetLat = lat; targetLon = lon; targetAlt = alt;
    }

    // Gimbal NED = aircraft NED + body-rotated mount offset.
    glm::vec3 PositionFrom(const glm::vec3& aircraftNed, const Aircraft& aircraft) const;

    // Target NED — uses named scene's GeoRef.
    glm::vec3 TargetInScene(const char* scene) const;

    // Integrate heading-relative joystick input into geodetic target.
    // Quadratic response, cos(lat) correction for east-west rate.
    void ApplyJoystickInput(glm::vec2 joy, float dt,
                            const Aircraft& aircraft, const Terrain& terrain);

    // Render sensor frustum + connecting lines (call inside Begin/End).
    // Uses current scene's GeoRef for target position.
    void DrawFrustum(const glm::vec3& aircraftNed, const Aircraft& aircraft) const;

    // Render clickable/draggable target marker (call inside Begin/End).
    // Drag updates target via terrain raycast.
    void DrawTargetMarker(const Terrain& terrain);

    void DrawControls(const Aircraft& aircraft, const Terrain& terrain);
};

} // namespace Kilo

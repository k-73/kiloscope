#pragma once
#include <glm/glm.hpp>

namespace Kilo {

class Aircraft;
class Terrain;

// Gimbal — sensor mounted on aircraft body, looking at a geodetic target.
// Holds references to its aircraft and terrain (injected at construction).
class Gimbal {
public:
    Gimbal(const Aircraft& aircraft, const Terrain& terrain)
        : aircraft_(aircraft), terrain_(terrain) {}

    // Config
    glm::vec3 bodyOffset = {0.0f, 0.f, 0.5f};  // body frame: X=fwd, Y=right, Z=down
    float     fov        = 50.f;
    float     aspect     = 16.f / 9.f;
    double    targetLat  = 52.2297, targetLon = 21.0122, targetAlt = 0.0;

    void SetTarget(double lat, double lon, double alt) {
        targetLat = lat; targetLon = lon; targetAlt = alt;
    }

    // Compute gimbal + target positions for current scene. Call once per scene per frame.
    void Update(const glm::vec3& aircraftNed, const char* scene);

    // Computed state (valid after Update)
    glm::vec3 position{0.f};
    glm::vec3 target{0.f};

    // Render (call inside Begin/End, after Update)
    void DrawFrustum(const glm::vec3& aircraftNed) const;
    void DrawTargetMarker();

    void DrawControls();

private:
    void ApplyJoystickInput(glm::vec2 joy, float dt);

    const Aircraft& aircraft_;
    const Terrain&  terrain_;
    glm::mat3       bodyToNed_{1.f};
};

} // namespace Kilo

#pragma once
#include "Render/Frame.hpp"
#include <glm/glm.hpp>

namespace Kilo {

// Aircraft — geodetic state, physics integration, body-frame model rendering.
// Caller (Airspace) gates HandleInput by focus + camera mode.
class Aircraft {
public:
    Aircraft();

    double lat = 52.2297, lon = 21.0122, alt = 20.0;
    float  yaw = 0.f, pitch = 0.f, roll = 0.f, speed = 0.f;

    void UpdatePhysics(float dt);
    void HandleInput(float dt, bool active);  // WASD pitch/yaw + bank; resets bank when !active

    void DrawAt(const glm::vec3& posNed) const;  // model + afterburner with attitude
    void DrawControls();

    glm::mat3 BodyToNed() const { return Render::EulerZYX(yaw, pitch, roll); }

private:
    float bank_ = 0.f;  // smoothed roll input from A/D
};

} // namespace Kilo

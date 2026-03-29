#pragma once
#include "Core/Panel/Panel.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace Kilo {

class Airspace : public Panel {
public:
    Airspace();
    void OnDraw() override;

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

    // Trail (ECEF cache — GeographicLib once on record, cheap matrix per frame)
    static constexpr size_t  kTrailMax  = 128;
    static constexpr double  kTrailStep = 1.0;   // meters between trail points
    std::vector<glm::dvec3>  trailEcef_;          // ECEF positions (computed once)
    std::vector<glm::vec3>   trailBuf_;           // NED positions (recomputed per frame)

};

} // namespace Kilo

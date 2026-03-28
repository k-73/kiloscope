#pragma once
#include "Core/Panel/Panel.hpp"
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
    void DrawAircraftModel(const glm::vec3& pos);
    void DrawFlight(const glm::vec3& nedPos);
    void DrawGimbal();
    void SetupEnv(const char* scene);

    // Aircraft (geodetic absolute)
    struct AircraftState {
        double lat = 52.2297, lon = 21.0122, alt = 20.0;
        float  yaw = 0.f, pitch = 0.f, roll = 0.f, speed = 0.f;
    } aircraft_;

    // Gimbal target (geodetic)
    struct GimbalTarget {
        double lat = 52.2297, lon = 21.0122, alt = 0.0;
    } gimbal_;

    struct CameraMode {
        bool chase = true;
        bool free  = false;
    } cameraMode_;

    // Trail (geodetic — survives origin shifts)
    static constexpr size_t kTrailMax = 128;
    std::vector<Render::GeoCoord> trail_;
    std::vector<glm::vec3> trailBuf_;
};

} // namespace Kilo

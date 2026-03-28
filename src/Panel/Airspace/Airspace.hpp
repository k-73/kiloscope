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
    void DrawWorld(const char* scene, const glm::vec3& pos);
    void SetupEnv(const char* scene);

    // Aircraft state (geodetic absolute — source of truth)
    struct AircraftState {
        double lat = 52.2297, lon = 21.0122, alt = 20.0;
        float  yaw   = 0.f;
        float  pitch = 0.f;
        float  roll  = 0.f;
        float  speed = 0.f;
    } aircraft_;

    struct CameraMode {
        bool chase = true;
        bool free  = false;
    } cameraMode_;

    // Trail stored in geodetic (survives origin shifts)
    static constexpr size_t kTrailMax = 128;
    std::vector<Render::GeoCoord> trail_;
    std::vector<glm::vec3> trailBuf_;  // reused buffer for NED conversion
};

} // namespace Kilo

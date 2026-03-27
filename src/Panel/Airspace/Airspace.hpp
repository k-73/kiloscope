#pragma once
#include "Core/Panel/Panel.hpp"
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
    void UpdatePhysics(float dt, bool focused);
    void DrawWorld(const char* scene);
    void SetupEnv(const char* scene);

    struct AircraftState {
        glm::vec3 position{0, 0, -2.f};
        float yaw   = 0.f;
        float pitch = 0.f;
        float roll  = 0.f;
        float speed = 0.f;
    } aircraft_;

    struct CameraMode {
        bool chase  = true;
        bool free   = false;
    } cameraMode_;

    static constexpr size_t kTrailMax = 300;
    std::vector<glm::vec3> trail_;
};

} // namespace Kilo

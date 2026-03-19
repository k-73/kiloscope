#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>

namespace Kilo {

class Airspace : public Panel {
public:
    Airspace();
    void OnDraw() override;

private:
    bool DrawControls();
    void HandleInput(float dt, bool focused);
    void UpdatePhysics(float dt, bool focused);
    void UpdateChaseCamera();
    void DrawScene();
    void CaptureChaseCamera();

    // Aircraft state
    glm::vec3 pos_{0, 0, -2.f};
    float yaw_   = 0.f;
    float pitch_ = 0.f;
    float roll_  = 0.f;
    float speed_ = 0.f;

    // Camera mode
    bool chase_   = true;
    bool freecam_ = false;

    // Chase camera offsets (accumulated from user orbit/zoom)
    float chaseYawOff_   = 0.f;
    float chasePitchOff_ = 0.f;
    float chaseDist_     = 4.f;
    float prevYaw_       = 0.f;
    float prevPitch_     = 0.f;
};

} // namespace Kilo

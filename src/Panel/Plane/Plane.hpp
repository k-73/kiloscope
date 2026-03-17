#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>

namespace Kilo {

class PlanePanel : public Panel {
public:
    PlanePanel();
    void OnDraw() override;

private:
    void DrawControlsWindow();
    void HandleInput(float dt, bool focused);
    void UpdatePhysics(float dt, bool focused);
    void UpdateChaseCamera();
    void DrawScene();
    void CaptureChaseCamera();

    glm::vec3 pos_{0, 0, -2.f};
    float yaw_ = 0.f, pitch_ = 0.f, roll_ = 0.f, speed_ = 0.f;
    bool  chase_   = true;
    bool  freecam_ = false;

    // Chase camera: MMB-orbit offsets and scroll distance, captured from Draw.cpp
    float chaseYawOff_   = 0.f;
    float chasePitchOff_ = 0.f;
    float chaseDist_     = 4.f;
    float camYawPrev_    = 0.f;
    float camPitchPrev_  = 0.f;
};

} // namespace Kilo

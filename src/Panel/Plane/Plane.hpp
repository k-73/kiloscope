#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>
#include <chrono>

namespace Kilo {

class PlanePanel : public Panel {
public:
    PlanePanel();
    void OnLoop() override;
    void OnDraw() override;

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_ = Clock::now();
    float t_ = 0.f, prevT_ = 0.f;

    glm::vec3 pos_{0, 0, -2.f};
    float yaw_ = 0.f, pitch_ = 0.f, roll_ = 0.f;
    float speed_ = 0.f;
    bool  chase_   = true;
    bool  freecam_ = false;

    // Chase camera: accumulated offsets from MMB drag + scroll zoom
    float chaseYawOff_   = 0.f;
    float chasePitchOff_ = 0.f;
    float chaseDist_     = 4.f;
    float camYawPrev_    = 0.f;
    float camPitchPrev_  = 0.f;
};

} // namespace Kilo

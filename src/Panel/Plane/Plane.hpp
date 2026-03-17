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

    glm::vec3 pos_{0, 0, -2.f};   // NED position (Z negative = above ground)
    float yaw_ = 0.f, pitch_ = 0.f, roll_ = 0.f;  // deg
    float speed_ = 0.f;                             // viewport units/s
    bool  chase_ = true;
};

} // namespace Kilo

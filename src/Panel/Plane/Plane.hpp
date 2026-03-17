#pragma once
#include "Core/Panel/Panel.hpp"
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
    float t_ = 0.f;

    // Euler angles (deg) — NED convention: roll, pitch, yaw
    float roll_ = 0.f, pitch_ = 5.f, yaw_ = 0.f;
    float speed_ = 80.f;        // m/s
    float altitude_ = 1000.f;   // m (positive = up for display, stored as -NED_Z)
};

} // namespace Kilo

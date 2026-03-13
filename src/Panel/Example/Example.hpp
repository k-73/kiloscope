#pragma once
#include "Core/Panel/Panel.hpp"
#include <chrono>
#include <vector>

namespace Kilo {

class Example : public Panel {
public:
    Example();

    void OnLoop() override;
    void OnDraw() override;

private:
    static constexpr int kPlotPoints = 256;
    static constexpr float kPi = 3.14159265f;

    using Clock = std::chrono::steady_clock;
    Clock::time_point startTime_ = Clock::now();
    float elapsedTime_ = 0.f;

    std::vector<float> plotX_, plotSin_, plotCos_;
};

} // namespace Kilo

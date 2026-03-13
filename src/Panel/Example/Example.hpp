#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/gtc/constants.hpp>
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

    using Clock = std::chrono::steady_clock;
    Clock::time_point startTime_ = Clock::now();
    float elapsedTime_ = 0.f;

    std::vector<float> plotX_, plotSin_, plotCos_;
};

} // namespace Kilo

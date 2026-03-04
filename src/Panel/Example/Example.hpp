#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>
#include <chrono>
#include <vector>

namespace KiloScope {

class Example : public Panel {
public:
    Example();

    void OnLoop() override;
    void OnDraw() override;

private:
    static constexpr int kPathPoints = 512;
    static constexpr int kPlotPoints = 256;
    static constexpr float kPi = 3.14159265f;

    using Clock = std::chrono::steady_clock;
    Clock::time_point startTime_ = Clock::now();
    float elapsedTime_ = 0.f;

    std::vector<glm::vec3> spiralPath_;
    std::vector<float> plotX_;
    std::vector<float> plotSin_;
    std::vector<float> plotCos_;
};

} // namespace KiloScope

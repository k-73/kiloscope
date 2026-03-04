#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace KiloScope {

class Example : public Panel {
public:
    Example();

    void OnLoop() override;
    void OnDraw() override;

protected:
    void OnRender(Render::Scene& scene) override;

private:
    static constexpr size_t PathLen = 512;
    static constexpr size_t ChartLen = 256;

    double time_ = 0.0;

    // 3D path data (written by OnLoop, read by OnRender)
    std::vector<glm::vec3> path_;
    glm::vec3 tip_{};

    // Chart data (written by OnLoop, read by OnDraw)
    std::vector<float> chartX_;
    std::vector<float> chartSin_;
    std::vector<float> chartCos_;
};

} // namespace KiloScope

#include "Example.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Primitives.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>

namespace KiloScope {

Example::Example()
    : Panel("Example", "Example")
    , spiralPath_(kPathPoints)
    , plotX_(kPlotPoints)
    , plotSin_(kPlotPoints)
    , plotCos_(kPlotPoints)
{}

void Example::OnLoop() {
    elapsedTime_ = std::chrono::duration<float>(Clock::now() - startTime_).count();

    // Animated 3D spiral
    for (int i = 0; i < kPathPoints; ++i) {
        float progress = static_cast<float>(i) / kPathPoints;
        float angle    = elapsedTime_ + progress * 8.f;
        float radius   = 1.f + progress * 2.f;
        spiralPath_[i] = {radius * std::cos(angle),
                          progress * 4.f - 2.f,
                          radius * std::sin(angle)};
    }

    // Animated sin/cos signals
    constexpr float xRange = 4.f * kPi;
    for (int i = 0; i < kPlotPoints; ++i) {
        float x     = static_cast<float>(i) / kPlotPoints * xRange;
        plotX_[i]   = x;
        plotSin_[i] = std::sin(x + elapsedTime_);
        plotCos_[i] = std::cos(x + elapsedTime_ * 0.7f) * 0.8f;
    }
}

void Example::OnDraw() {
    Draw3D("spiral", {100, 100}, [&](Render::Primitives& prims) {
        prims.DrawAxes({0, 0, 0}, 1.f);

        for (int i = 1; i < kPathPoints; ++i) {
            float norm = static_cast<float>(i) / kPathPoints;
            prims.DrawLine(spiralPath_[i - 1], spiralPath_[i],
                           {.2f + .8f * norm, .4f, 1.f - .6f * norm, 1.f}, 2.f);
        }

        prims.DrawSphere(spiralPath_.back(), 0.12f, {1.f, .8f, .2f, 1.f}, 24);
    });

    ImGui::Begin("Example Chart", nullptr);
    if (ImPlot::BeginPlot("##signals", {-1, -1})) {
        ImPlot::SetupAxes("x", "y");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 4.0 * kPi, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.2, 1.2, ImPlotCond_Always);
        ImPlot::PlotLine("sin", plotX_.data(), plotSin_.data(), kPlotPoints);
        ImPlot::PlotLine("cos", plotX_.data(), plotCos_.data(), kPlotPoints);
        ImPlot::EndPlot();
    }
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Example>("Example", "Example");

} // namespace KiloScope

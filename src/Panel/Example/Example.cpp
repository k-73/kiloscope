#include "Example.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Scene.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>

namespace KiloScope {

Example::Example()
    : Panel("Example", "Example", PanelFlags::NeedsScene)
{
    path_.reserve(PathLen);
    chartX_.resize(ChartLen);
    chartSin_.resize(ChartLen);
    chartCos_.resize(ChartLen);
}

void Example::OnLoop() {
    time_ += 0.016;

    // Generate 3D spiral path
    path_.resize(PathLen);
    for (size_t i = 0; i < PathLen; ++i) {
        float t = static_cast<float>(i) / PathLen;
        float phase = static_cast<float>(time_) + t * 8.0f;
        float r = 1.0f + t * 2.0f;
        path_[i] = {
            r * std::cos(phase),
            t * 4.0f - 2.0f,
            r * std::sin(phase)
        };
    }
    tip_ = path_.back();

    // Generate chart data
    for (size_t i = 0; i < ChartLen; ++i) {
        float x = static_cast<float>(i) / ChartLen * 4.0f * 3.14159f;
        float phase = static_cast<float>(time_);
        chartX_[i]   = x;
        chartSin_[i] = std::sin(x + phase);
        chartCos_[i] = std::cos(x + phase * 0.7f) * 0.8f;
    }
}

void Example::OnRender(Render::Scene& scene) {
    auto& p = scene.Prims();
    p.DrawAxes({0, 0, 0}, 1.0f);

    for (size_t i = 1; i < path_.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(path_.size());
        p.DrawLine(path_[i - 1], path_[i],
                   {0.2f + 0.8f * t, 0.4f, 1.0f - 0.6f * t, 1.0f}, 2.0f);
    }

    if (!path_.empty())
        p.DrawSphere(tip_, 0.12f, {1.0f, 0.8f, 0.2f, 1.0f}, 24);
}

void Example::OnDraw() {
    DrawViewport();

    // Second window: chart
    bool open = true;
    ImGui::Begin("Example Chart", &open);
    if (ImPlot::BeginPlot("##signals", {-1, -1})) {
        ImPlot::SetupAxes("x", "y");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 4.0 * 3.14159, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.2, 1.2, ImPlotCond_Always);
        ImPlot::PlotLine("sin", chartX_.data(), chartSin_.data(), static_cast<int>(ChartLen));
        ImPlot::PlotLine("cos", chartX_.data(), chartCos_.data(), static_cast<int>(ChartLen));
        ImPlot::EndPlot();
    }
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Example>("Example", "Example");

} // namespace KiloScope

#include "Example.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Primitives.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>

namespace Kilo {

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
    Draw3D("scene", {}, [&](Render::Primitives& p, Render::Camera& cam) {
        p.DrawAxes({0, 0, 0}, 1.f);

        // Animated spiral path
        for (int i = 1; i < kPathPoints; ++i) {
            float norm = static_cast<float>(i) / kPathPoints;
            p.DrawLine(spiralPath_[i - 1], spiralPath_[i],
                       {.2f + .8f * norm, .4f, 1.f - .6f * norm, 1.f}, 2.f);
        }
        p.DrawSphere(spiralPath_.back(), 0.12f, {1.f, .8f, .2f, 1.f}, 24);

        float t = elapsedTime_;

        // Box
        p.PushMatrix();
            p.Translate(-2, 0, 0);
            p.DrawCube({0, 0, 0}, 0.3f, {.4f, .7f, .9f, 1});
        p.PopMatrix();

        // Cone
        p.PushMatrix();
            p.Translate(2, 0, 0);
            p.DrawCone({0, -0.5f, 0}, {0, 0.5f, 0}, 0.25f, {.9f, .5f, .3f, 1});
        p.PopMatrix();

        // Capsule
        p.PushMatrix();
            p.Translate(0, -2, 0);
            p.DrawCapsule({0, 0, -1}, {0, 0, 1}, 0.15f, {.6f, .9f, .5f, 1});
        p.PopMatrix();

        // Torus + animated circle
        p.PushMatrix();
            p.Translate(0, 2, 0);
            p.DrawTorus({0, 0, 0}, {0, 1, 0}, 0.6f, 0.15f, {.8f, .4f, .8f, 1});
            p.DrawCircle({0, 0, 0}, {0, 1, 0}, 1.0f + 0.2f * std::sin(t), {1, 1, 1, .5f});
        p.PopMatrix();

        // Disk
        p.PushMatrix();
            p.Translate(-2, 1.5f, 0);
            p.DrawDisk({0, 0, 0}, {0, 1, 0}, 0.4f, {.3f, .8f, .7f, 1});
        p.PopMatrix();

        // Ring
        p.PushMatrix();
            p.Translate(2, 1.5f, 0);
            p.DrawRing({0, 0, 0}, {0, 1, 0}, 0.2f, 0.4f, {.9f, .7f, .3f, 1});
        p.PopMatrix();

        // Ground plane
        p.PushMatrix();
            p.Translate(0, -2.5f, 0);
            p.DrawPlane({0, 0, 0}, {0, 1, 0}, {2, 2}, {.5f, .5f, .5f, .6f});
        p.PopMatrix();
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

} // namespace Kilo

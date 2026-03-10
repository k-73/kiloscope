#include "Example.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Camera.hpp"
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
    Draw3D("scene", [&] {
        Render::Axes({0, 0, 0}, 1.f);

        // Animated spiral path
        for (int i = 1; i < kPathPoints; ++i) {
            float norm = static_cast<float>(i) / kPathPoints;
            Render::Line(spiralPath_[i - 1], spiralPath_[i],
                         {.2f + .8f * norm, .4f, 1.f - .6f * norm, 1.f}, 2.f);
        }
        Render::Sphere(spiralPath_.back(), 0.12f, {1.f, .8f, .2f, 1.f}, 24);

        Render::PushMatrix();
            Render::Translate(-2, 0, 0);
            Render::Cube({0, 0, 0}, 0.3f, {.4f, .7f, .9f, 1});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(2, 0, 0);
            Render::Cone({0, -0.5f, 0}, {0, 0.5f, 0}, 0.25f, {.9f, .5f, .3f, 1});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(0, -2, 0);
            Render::Capsule({0, 0, -1}, {0, 0, 1}, 0.15f, {.6f, .9f, .5f, 1});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(0, 2, 0);
            Render::Torus({0, 0, 0}, {0, 1, 0}, 0.6f, 0.15f, {.8f, .4f, .8f, 1});
            Render::Circle({0, 0, 0}, {0, 1, 0}, 1.0f + 0.2f * std::sin(elapsedTime_), {1, 1, 1, .5f});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(-2, 1.5f, 0);
            Render::Disk({0, 0, 0}, {0, 1, 0}, 0.4f, {.3f, .8f, .7f, 1});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(2, 1.5f, 0);
            Render::Ring({0, 0, 0}, {0, 1, 0}, 0.2f, 0.4f, {.9f, .7f, .3f, 1});
        Render::PopMatrix();

        Render::PushMatrix();
            Render::Translate(0, -2.5f, 0);
            Render::Plane({0, 0, 0}, {0, 1, 0}, {2, 2}, {.5f, .5f, .5f, .6f});
        Render::PopMatrix();
    });

    // Camera controls
    auto& cam = Render::GetCamera();
    if (ImGui::TreeNode("Camera")) {
        ImGui::DragFloat3("Target",   &cam.Target().x, 0.05f);
        ImGui::DragFloat("Distance",  &cam.Distance(), 0.1f, 0.5f, 200.f);
        ImGui::SliderFloat("Yaw",     &cam.Yaw(), -180.f, 180.f, "%.1f\xc2\xb0");
        ImGui::SliderFloat("Pitch",   &cam.Pitch(), -89.f, 89.f, "%.1f\xc2\xb0");
        auto pos = cam.Position();
        ImGui::Text("Position: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
        if (ImGui::Button("Reset")) {
            cam.Target()   = {0.f, 0.f, 0.f};
            cam.Distance() = 8.f;
            cam.Yaw()      = 45.f;
            cam.Pitch()    = 30.f;
        }
        ImGui::TreePop();
    }

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

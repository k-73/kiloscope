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
    , plotX_(kPlotPoints)
    , plotSin_(kPlotPoints)
    , plotCos_(kPlotPoints)
{}

void Example::OnLoop() {
    elapsedTime_ = std::chrono::duration<float>(Clock::now() - startTime_).count();

    constexpr float xRange = 4.f * kPi;
    for (int i = 0; i < kPlotPoints; ++i) {
        float x     = static_cast<float>(i) / kPlotPoints * xRange;
        plotX_[i]   = x;
        plotSin_[i] = std::sin(x + elapsedTime_);
        plotCos_[i] = std::cos(x + elapsedTime_ * 0.7f) * 0.8f;
    }
}

void Example::OnDraw() {
    float t = elapsedTime_;

    Render::Begin("scene", {.width = 600, .height = 600});
        Render::Grid();

        // Central body
        Render::Sphere({0, 0, 0}, 0.4f, {1.f, .85f, .4f, 1.f});

        // Orbit 1 — equatorial
        float r1 = 2.2f, a1 = t * 0.8f;
        Render::Circle({0, 0, 0}, {0, 0, 1}, r1, {.4f, .65f, 1.f, .25f}, 64, 1.f);
        Render::Sphere({r1 * std::cos(a1), r1 * std::sin(a1), 0}, 0.18f, {.4f, .65f, 1.f, 1.f});

        // Orbit 2 — tilted 65°
        Render::PushMatrix();
        Render::RotateX(65.f);
            float r2 = 3.f, a2 = t * 0.5f;
            Render::Circle({0, 0, 0}, {0, 0, 1}, r2, {.95f, .45f, .5f, .25f}, 64, 1.f);
            Render::Sphere({r2 * std::cos(a2), r2 * std::sin(a2), 0}, 0.14f, {.95f, .5f, .55f, 1.f});
        Render::PopMatrix();

        // Orbit 3 — opposite tilt
        Render::PushMatrix();
        Render::RotateY(55.f);
            float r3 = 1.6f, a3 = t * 1.2f;
            Render::Circle({0, 0, 0}, {0, 0, 1}, r3, {.3f, .85f, .65f, .25f}, 64, 1.f);
            Render::Sphere({r3 * std::cos(a3), r3 * std::sin(a3), 0}, 0.11f, {.3f, .85f, .7f, 1.f});
        Render::PopMatrix();
    Render::End();

    ImGui::Begin("Environment", nullptr);
        auto& env = Render::GetEnvironment("scene");
        ImGui::DragFloat3("Light Dir", &env.lightDir.x, 0.05f);
        ImGui::ColorEdit3("BG Color", &env.bgColor.x);
        ImGui::DragFloat("Ambient",     &env.ambient,    0.01f, 0.f, 1.f);
        ImGui::DragFloat("Diffuse",     &env.diffuse,    0.01f, 0.f, 1.f);
        ImGui::DragFloat("Roughness",   &env.roughness,  0.01f, 0.f, 1.f);
        ImGui::DragFloat("Specular",    &env.specular,   0.01f, 0.f, 1.f);
        ImGui::DragFloat("Fresnel",     &env.fresnel,    0.01f, 0.f, 1.f);
        ImGui::DragFloat("Fog Density", &env.fogDensity, 0.00001f, 0.f, 0.01f);
        ImGui::Checkbox("Show Sun", &env.showSun);
    ImGui::End();

    ImGui::Begin("Grid", nullptr);
        auto& grid = Render::GetGrid("scene");
        ImGui::Checkbox("Enabled", &grid.enabled);
        ImGui::DragFloat("Scale Fine",   &grid.scaleFine,   0.1f, 0.1f, 10.f);
        ImGui::DragFloat("Scale Medium", &grid.scaleMedium, 1.f,  1.f,  100.f);
        ImGui::DragFloat("Scale Coarse", &grid.scaleCoarse, 10.f, 10.f, 1000.f);
        ImGui::ColorEdit3("Color Fine",   &grid.colorFine.x);
        ImGui::ColorEdit3("Color Medium", &grid.colorMedium.x);
        ImGui::ColorEdit3("Color Coarse", &grid.colorCoarse.x);
        ImGui::ColorEdit3("Axis X", &grid.axisXColor.x);
        ImGui::ColorEdit3("Axis Y", &grid.axisYColor.x);
        ImGui::DragFloat("Axis Thickness", &grid.axisThickness, 0.001f, 0.001f, 0.1f);
        ImGui::DragFloat("Fade Start", &grid.fadeStart, 0.1f, 0.1f, 20.f);
        ImGui::DragFloat("Fade End",   &grid.fadeEnd,   0.1f, 1.f,  50.f);
    ImGui::End();

    ImGui::Begin("Camera", nullptr);
        auto& cam = Render::GetCamera("scene");
        ImGui::DragFloat3("Target",   &cam.Target().x, 0.05f);
        ImGui::DragFloat("Distance",  &cam.Distance(), 0.1f, 0.5f, 200.f);
        ImGui::SliderFloat("Yaw",     &cam.Yaw(), -180.f, 180.f, "%.1f\xc2\xb0");
        ImGui::SliderFloat("Pitch",   &cam.Pitch(), -89.f, 89.f, "%.1f\xc2\xb0");
        auto pos = cam.Position();
        ImGui::Text("Pos: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
        if (ImGui::Button("Reset")) {
            cam.Target()   = {0.f, 0.f, 0.f};
            cam.Distance() = 8.f;
            cam.Yaw()      = 45.f;
            cam.Pitch()    = 30.f;
        }
    ImGui::End();

    ImGui::Begin("Signals", nullptr);
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

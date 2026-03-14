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

    constexpr float xRange = 4.f * glm::pi<float>();
    for (int i = 0; i < kPlotPoints; ++i) {
        float x     = static_cast<float>(i) / kPlotPoints * xRange;
        plotX_[i]   = x;
        plotSin_[i] = std::sin(x + elapsedTime_);
        plotCos_[i] = std::cos(x + elapsedTime_ * 0.7f) * 0.8f;
    }
}

void Example::OnDraw() {
    using namespace Render::Color;
    float time = elapsedTime_;

    Render::Begin("scene", {.width = 600, .height = 600});
        Render::Grid();
        Render::PointLight({0, 0, 0}, {1.f, .95f, .8f}, 6.f);

        // Origin frame
        static bool showFrame = true;
        if (showFrame) {
            Render::Frame(glm::mat4(1.f), 1.f);
            if (Render::Event().Clicked())
                showFrame = false;
        }

        // Star (emissive core + glow rings)
        Render::SetNextEmissive();
        Render::Sphere({0, 0, 0}, 0.4f, Hex("#F2EB4D"));
        auto starEvent = Render::Event();
        if (starEvent.Hovered())
            Render::Text({0, 0, 0.7f}, White, "star");
        if (starEvent.Clicked())
            ImGui::OpenPopup("StarInfo");

        // Planet 1 — equatorial orbit
        constexpr float orbit1R = 2.2f, planet1R = 0.18f;
        float orbit1Angle = time * 0.8f;
        glm::vec3 planet1Pos = {orbit1R * std::cos(orbit1Angle), orbit1R * std::sin(orbit1Angle), 0.f};
        Render::Circle({0, 0, 0}, {0, 0, 1}, orbit1R, Hex("#5980F240"), 64, 1.f);
        Render::Sphere(planet1Pos, planet1R, Hex("#5980F2"));

        // Planet 2 — tilted 65°
        constexpr float orbit2R = 3.0f, planet2R = 0.14f;
        float orbit2Angle = time * 0.5f;
        Render::PushMatrix();
        Render::RotateX(65.f);
            Render::Circle({0, 0, 0}, {0, 0, 1}, orbit2R, Hex("#F2404040"), 64, 1.f);
            Render::Sphere({orbit2R * std::cos(orbit2Angle), orbit2R * std::sin(orbit2Angle), 0.f}, planet2R, Hex("#F24040"));
        Render::PopMatrix();

        // Moon — tilted 55°
        constexpr float orbit3R = 1.6f, moonR = 0.11f;
        float orbit3Angle = time * 1.2f;
        Render::PushMatrix();
        Render::RotateY(55.f);
            Render::Circle({0, 0, 0}, {0, 0, 1}, orbit3R, Hex("#59D95940"), 64, 1.f);
            Render::Sphere({orbit3R * std::cos(orbit3Angle), orbit3R * std::sin(orbit3Angle), 0.f}, moonR, Hex("#59D959"));
        Render::PopMatrix();

        // Axis labels
        Render::Text({1.1f, 0, 0}, Hex("#E65555"), "X");
        Render::Text({0, 1.1f, 0}, Hex("#55CC55"), "Y");
        Render::Text({0, 0, 1.1f}, Hex("#5580E6"), "Z");
    Render::End();

    // Star glow — screen-space radial gradient
    {
        auto s = Render::WorldToScreen({0, 0, 0});
        if (s.x > 0.f) {
            auto* dl = ImGui::GetWindowDrawList();
            constexpr float glowR = 70.f;
            constexpr int steps = 30;
            for (int i = steps; i >= 0; --i) {
                float t = static_cast<float>(i) / steps;
                float a = (1.f - t * t) * 0.06f;
                dl->AddCircleFilled({s.x, s.y}, glowR * t,
                    ImGui::ColorConvertFloat4ToU32({1.f, .92f, .5f, a}), 32);
            }
        }
    }

    if (ImGui::BeginPopup("StarInfo")) {
        ImGui::SeparatorText("Star");
        ImGui::Spacing();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

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
        if (env.showSun) {
            ImGui::DragFloat("Sun Distance", &env.sunDistance, 0.5f, 1.f, 100.f);
            ImGui::DragFloat("Sun Radius",   &env.sunRadius,  0.05f, 0.05f, 5.f);
        }
    ImGui::End();

    ImGui::Begin("Grid", nullptr);
        auto& grid = Render::GetGrid("scene");
        ImGui::Checkbox("Enabled", &grid.enabled);
        ImGui::DragFloat("Scale Fine",   &grid.scaleFine,   0.1f, 0.1f, 10.f);
        ImGui::DragFloat("Scale Medium", &grid.scaleMedium, 1.f,  1.f,  100.f);
        ImGui::DragFloat("Scale Coarse", &grid.scaleCoarse, 10.f, 10.f, 1000.f);
        ImGui::ColorEdit4("Color Fine",   &grid.colorFine.x);
        ImGui::ColorEdit4("Color Medium", &grid.colorMedium.x);
        ImGui::ColorEdit4("Color Coarse", &grid.colorCoarse.x);
        ImGui::Separator();
        ImGui::ColorEdit4("Axis X", &grid.axisXColor.x);
        ImGui::ColorEdit4("Axis Y", &grid.axisYColor.x);
        ImGui::DragFloat("Axis Thickness", &grid.axisThickness, 0.001f, 0.001f, 0.5f);
        ImGui::Checkbox("Axis Scale With Cam", &grid.axisScaleWithCam);
        ImGui::Separator();
        ImGui::DragFloat("Fade Start", &grid.fadeStart, 0.1f, 0.1f, 20.f);
        ImGui::DragFloat("Fade End",   &grid.fadeEnd,   0.1f, 1.f,  50.f);
    ImGui::End();

    ImGui::Begin("Camera", nullptr);
        auto& cam = Render::GetCamera("scene");
        auto eye = cam.Position();
        ImGui::Text("Eye:   %.1f, %.1f, %.1f", eye.x, eye.y, eye.z);
        ImGui::Text("Pivot: %.1f, %.1f, %.1f", cam.Pivot().x, cam.Pivot().y, cam.Pivot().z);
        ImGui::DragFloat3("Pivot",    &cam.Target().x, 0.05f);
        ImGui::DragFloat("Distance",  &cam.Distance(), 0.1f, 0.01f, 1000.f);
        ImGui::SliderFloat("Yaw",     &cam.Yaw(), -180.f, 180.f, "%.1f\xc2\xb0");
        ImGui::SliderFloat("Pitch",   &cam.Pitch(), -89.f, 89.f, "%.1f\xc2\xb0");
        ImGui::SliderFloat("FOV",     &cam.Fov(), 10.f, 120.f, "%.0f\xc2\xb0");
        if (ImGui::Button("Reset")) {
            cam.Target()   = {0.f, 0.f, 0.f};
            cam.Distance() = 8.f;
            cam.Yaw()      = 45.f;
            cam.Pitch()    = 30.f;
            cam.Fov()      = 45.f;
        }
    ImGui::End();

    ImGui::Begin("Signals", nullptr);
    if (ImPlot::BeginPlot("##signals", {-1, -1})) {
        ImPlot::SetupAxes("x", "y");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 4.0 * glm::pi<float>(), ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.2, 1.2, ImPlotCond_Always);
        ImPlot::PlotLine("sin", plotX_.data(), plotSin_.data(), kPlotPoints);
        ImPlot::PlotLine("cos", plotX_.data(), plotCos_.data(), kPlotPoints);
        ImPlot::EndPlot();
    }
    ImGui::End();
}

static const bool reg_ = RegisterPanel<Example>("Example", "Example");

} // namespace Kilo

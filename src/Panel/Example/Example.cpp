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

        // Origin frame
        static bool showFrame = true;
        if (showFrame) {
            Render::Frame(glm::mat4(1.f), 1.f);
            if (Render::Event().Clicked())
                showFrame = false;
        }

        // Star
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

    // ── Scene 2: Lighting demo ──────────────────────────────────
    ImGui::SetNextWindowSize({500, 500}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting Demo", nullptr);
        Render::Begin("lighting");
            Render::Grid();
            Render::Box({0, 0, -0.05f}, {12, 12, 0.1f}, Hex("#333333"));

            // Central tower — rainbow torus stack
            Render::Cylinder({0, 0, 0}, {0, 0, 2.f}, 0.1f, Hex("#606060"));
            for (int i = 0; i < 5; ++i) {
                float z = 0.3f + i * 0.4f, r = 0.5f - i * 0.04f;
                Render::Torus({0, 0, z}, {0, 0, 1}, r, 0.035f, Hue(i / 5.f));
            }
            Render::SetNextEmissive();
            Render::Sphere({0, 0, 2.15f}, 0.15f, Hex("#FFD700"));

            // Animated hexagonal pillars
            for (int i = 0; i < 6; ++i) {
                float a = i * glm::pi<float>() / 3.f;
                float x = 3.f * std::cos(a), y = 3.f * std::sin(a);
                float h = 0.6f + 0.3f * std::sin(time * 0.8f + a);
                Render::Cylinder({x, y, 0}, {x, y, h}, 0.08f, Hue(i / 6.f + 0.1f));
                Render::Sphere({x, y, h + 0.08f}, 0.11f, Hue(i / 6.f + 0.1f));
            }

            // Orbiting lights
            for (int i = 0; i < 4; ++i) {
                float p = i * glm::two_pi<float>() / 4.f;
                float a = time * 0.8f + p;
                glm::vec3 lp = {1.6f * std::cos(a), 1.6f * std::sin(a),
                                0.8f + 0.4f * std::sin(time + p)};
                Render::SphereLight(lp, 0.04f, Hue(i / 4.f + time * 0.1f), 6.f);
            }

            // Ascending spiral of spheres
            for (int i = 0; i < 18; ++i) {
                float t = i / 18.f;
                float a = t * glm::two_pi<float>() * 2.5f + time * 0.3f;
                float r = 0.8f + t * 1.8f, z = t * 2.f;
                Render::Sphere({r * std::cos(a), r * std::sin(a), z},
                               0.04f + t * 0.04f, Hue(t + time * 0.05f));
            }
        Render::End();
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

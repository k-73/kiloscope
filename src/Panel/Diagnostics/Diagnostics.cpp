#include "Diagnostics.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include <imgui.h>
#include <implot.h>
#include <GL/glew.h>

namespace Kilo {

void Diagnostics::OnDraw() {
    auto& io = ImGui::GetIO();

    frameTimes_[frameIdx_] = 1000.f / io.Framerate;
    frameIdx_ = (frameIdx_ + 1) % 128;

    ImGui::SeparatorText("Performance");
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.f / io.Framerate);
    ImGui::PlotLines("##ft", frameTimes_, 128, frameIdx_, nullptr, 0.f, 33.f, {-1, 40});

    ImGui::SeparatorText("Render");
    auto& s = Render::GetStats();
    ImGui::Text("Draw Calls: %d (+%d pick)", s.drawCalls, s.pickDrawCalls);
    ImGui::Text("Vertices: %d  Lines: %d  Points: %d", s.vertices, s.lineSegments, s.points);
    ImGui::Text("Text Labels: %d", s.textLabels);
    ImGui::Text("Viewport: %dx%d  MSAA: %dx", s.viewportW, s.viewportH, s.msaaSamples);

    ImGui::SeparatorText("GPU");
    ImGui::Text("Renderer: %s", glGetString(GL_RENDERER));
    ImGui::Text("ImGui Vtx: %d  Idx: %d  Windows: %d",
        io.MetricsRenderVertices, io.MetricsRenderIndices, io.MetricsRenderWindows);

    ImGui::SeparatorText("Demos");
    ImGui::Checkbox("ImGui Demo", &showImGuiDemo_);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot Demo", &showImPlotDemo_);

    if (showImGuiDemo_)  ImGui::ShowDemoWindow(&showImGuiDemo_);
    if (showImPlotDemo_) ImPlot::ShowDemoWindow(&showImPlotDemo_);
}

static const bool reg_ = RegisterPanel<Diagnostics>("Diagnostics", "Diagnostics");

} // namespace Kilo

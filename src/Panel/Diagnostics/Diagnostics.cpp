#include "Diagnostics.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <implot.h>

namespace Kilo {

void Diagnostics::OnDraw() {
    auto& io = ImGui::GetIO();

    frameTimes_[frameIdx_] = 1000.f / io.Framerate;
    frameIdx_ = (frameIdx_ + 1) % 128;

    ImGui::SeparatorText("Performance");
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.f / io.Framerate);
    ImGui::PlotLines("##ft", frameTimes_, 128, frameIdx_, nullptr, 0.f, 33.f, {-1, 40});
    ImGui::Text("Windows: %d  Vtx: %d  Idx: %d",
        io.MetricsRenderWindows, io.MetricsRenderVertices, io.MetricsRenderIndices);

    ImGui::SeparatorText("Demos");
    ImGui::Checkbox("ImGui Demo", &showImGuiDemo_);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot Demo", &showImPlotDemo_);

    if (showImGuiDemo_)  ImGui::ShowDemoWindow(&showImGuiDemo_);
    if (showImPlotDemo_) ImPlot::ShowDemoWindow(&showImPlotDemo_);
}

static const bool reg_ = RegisterPanel<Diagnostics>("Diagnostics", "Diagnostics");

} // namespace Kilo

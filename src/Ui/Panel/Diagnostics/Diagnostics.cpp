#include "Diagnostics.hpp"
#include "Ui/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <implot.h>
#include <cinttypes>

namespace KiloScope::UI {

void Diagnostics::OnDraw() {
    auto& io = ImGui::GetIO();

    frameTimes_[frameIdx_] = 1000.f / io.Framerate;
    frameIdx_ = (frameIdx_ + 1) % 128;

    ImGui::SeparatorText("Performance");
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.f / io.Framerate);
    ImGui::PlotLines("##ft", frameTimes_, 128, frameIdx_, nullptr, 0.f, 33.f, {-1, 40});
    ImGui::Text("Windows: %d  Vtx: %d  Idx: %d",
        io.MetricsRenderWindows, io.MetricsRenderVertices, io.MetricsRenderIndices);

    ImGui::SeparatorText("Data");
    ImGui::Text("Packets: %" PRIu64 "  Samples: %" PRIu64,
        store_->TotalPackets(), store_->TotalSamples());
    auto ids = store_->ChannelIds();
    ImGui::Text("Channels: %zu  Buffer cap: %zu", ids.size(), Data::RingBuffer<Data::Sample>::Capacity());

    ImGui::SeparatorText("Demos");
    ImGui::Checkbox("ImGui Demo", &showImGuiDemo_);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot Demo", &showImPlotDemo_);

    if (showImGuiDemo_)  ImGui::ShowDemoWindow(&showImGuiDemo_);
    if (showImPlotDemo_) ImPlot::ShowDemoWindow(&showImPlotDemo_);
}

REGISTER_PANEL(Diagnostics, "Diagnostics", "Diagnostics",
    KiloScope::UI::PanelFlags::Singleton | KiloScope::UI::PanelFlags::NoSettings)

} // namespace KiloScope::UI

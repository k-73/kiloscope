#include "Histogram.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <implot.h>

namespace KiloScope {

void Histogram::OnData() {
    histData_.clear();
    size_t totalUsed = 0;

    for (auto id : store_->ChannelIds()) {
        auto* ch = store_->GetChannel(id);
        if (!ch) continue;
        auto count = ch->ReadLast(buf_.data(), MaxDisplay);
        if (!count) continue;

        size_t needed = totalUsed + count;
        if (needed > vals_.size()) vals_.resize(needed);

        for (size_t i = 0; i < count; ++i)
            vals_[totalUsed + i] = buf_[i].value;

        histData_.push_back({count, ch->Name(), totalUsed});
        totalUsed += count;
    }
}

void Histogram::OnDraw() {
    ImGui::SliderInt("Bins", &bins_, 8, 256);

    if (ImPlot::BeginPlot("##hist", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Value", "Count");
        for (auto& hd : histData_) {
            ImPlot::PlotHistogram(hd.name.c_str(),
                vals_.data() + hd.offset, (int)hd.count, bins_);
        }
        ImPlot::EndPlot();
    }
}

json Histogram::SaveSettings() const {
    return {{"bins", bins_}};
}

void Histogram::LoadSettings(const json& j) {
    if (j.contains("bins")) bins_ = j["bins"].get<int>();
}

REGISTER_PANEL(Histogram, "Histogram", "Histogram", KiloScope::PanelFlags::None)

} // namespace KiloScope

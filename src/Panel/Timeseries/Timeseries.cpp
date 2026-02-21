#include "Timeseries.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <implot.h>

namespace KiloScope {

void Timeseries::OnData(Data::DataStore& store) {
    plotData_.clear();
    offsets_.clear();
    size_t totalUsed = 0;

    for (auto id : store.ChannelIds()) {
        auto* ch = store.GetChannel(id);
        if (!ch) continue;
        auto count = ch->ReadLast(buf_.data(), MaxDisplay);
        if (!count) continue;

        size_t needed = totalUsed + count;
        if (needed > xs_.size()) { xs_.resize(needed); ys_.resize(needed); }

        double tLatest = buf_[count - 1].timestamp;
        for (size_t i = 0; i < count; ++i) {
            xs_[totalUsed + i] = buf_[i].timestamp - tLatest;
            ys_[totalUsed + i] = buf_[i].value;
        }

        offsets_.push_back(totalUsed);
        plotData_.push_back({count, ch->Name()});
        totalUsed += count;
    }
}

void Timeseries::OnDraw() {
    ImGui::SliderFloat("History (s)", &historySec_, 1.f, 60.f);

    if (ImPlot::BeginPlot("##ts", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Time (s)", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, -historySec_, 0, ImPlotCond_Always);

        for (size_t i = 0; i < plotData_.size(); ++i) {
            auto& pd = plotData_[i];
            ImPlot::PlotLine(pd.name.c_str(),
                xs_.data() + offsets_[i],
                ys_.data() + offsets_[i],
                (int)pd.count);
        }
        ImPlot::EndPlot();
    }
}

json Timeseries::SaveSettings() const {
    return {{"historySec", historySec_}};
}

void Timeseries::LoadSettings(const json& j) {
    if (j.contains("historySec")) historySec_ = j["historySec"].get<float>();
}

REGISTER_PANEL(Timeseries, "Timeseries", "Time Series", KiloScope::PanelFlags::None)

} // namespace KiloScope

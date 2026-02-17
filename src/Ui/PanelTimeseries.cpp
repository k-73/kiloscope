#include "PanelTimeseries.hpp"
#include <imgui.h>
#include <implot.h>

namespace ks::ui {

void PanelTimeseries::Draw() {
    ImGui::Begin(title_.c_str(), &visible_);
    ImGui::SliderFloat("History (s)", &historySec_, 1.f, 60.f);

    if (ImPlot::BeginPlot("##ts", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Time (s)", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, -historySec_, 0, ImPlotCond_Always);

        std::shared_lock lk(store_->Mutex());
        for (auto id : store_->ChannelIds()) {
            auto* ch = store_->GetChannel(id);
            if (!ch) continue;
            auto count = ch->ReadLast(buf_.data(), MaxDisplay);
            if (!count) continue;

            double tLatest = buf_[count - 1].timestamp;
            std::vector<double> xs(count), ys(count);
            for (size_t i = 0; i < count; ++i) {
                xs[i] = buf_[i].timestamp - tLatest;
                ys[i] = buf_[i].value;
            }
            ImPlot::PlotLine(ch->Name().c_str(), xs.data(), ys.data(), (int)count);
        }
        ImPlot::EndPlot();
    }
    ImGui::End();
}

} // namespace ks::ui

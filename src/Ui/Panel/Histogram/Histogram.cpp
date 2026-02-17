#include "Histogram.hpp"
#include <imgui.h>
#include <implot.h>

namespace ks::ui {

void Histogram::Draw() {
    ImGui::Begin(title_.c_str(), &visible_);
    ImGui::SliderInt("Bins", &bins_, 8, 256);

    if (ImPlot::BeginPlot("##hist", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Value", "Count");
        std::shared_lock lk(store_->Mutex());
        for (auto id : store_->ChannelIds()) {
            auto* ch = store_->GetChannel(id);
            if (!ch) continue;
            auto count = ch->ReadLast(buf_.data(), MaxDisplay);
            if (!count) continue;
            std::vector<double> vals(count);
            for (size_t i = 0; i < count; ++i) vals[i] = buf_[i].value;
            ImPlot::PlotHistogram(ch->Name().c_str(), vals.data(), (int)count, bins_);
        }
        ImPlot::EndPlot();
    }
    ImGui::End();
}

} // namespace ks::ui

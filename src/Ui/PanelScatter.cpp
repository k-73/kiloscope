#include "PanelScatter.hpp"
#include <imgui.h>
#include <implot.h>
#include <algorithm>

namespace ks::ui {

void PanelScatter::Draw() {
    ImGui::Begin(title_.c_str(), &visible_);

    auto ids = store_->ChannelIds();
    if (ids.size() < 2) {
        ImGui::TextWrapped("Need at least 2 channels.");
        ImGui::End(); return;
    }

    auto combo = [&](const char* label, int& sel) {
        if (ImGui::BeginCombo(label, ("ch" + std::to_string(sel)).c_str())) {
            for (auto id : ids) {
                if (ImGui::Selectable(("ch" + std::to_string(id)).c_str(), sel == id))
                    sel = id;
            }
            ImGui::EndCombo();
        }
    };
    combo("X", chX_); ImGui::SameLine(); combo("Y", chY_);

    if (ImPlot::BeginPlot("##scatter", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("X", "Y");
        std::shared_lock lk(store_->Mutex());
        auto* cx = store_->GetChannel((uint16_t)chX_);
        auto* cy = store_->GetChannel((uint16_t)chY_);
        if (cx && cy) {
            auto n = std::min(cx->ReadLast(bufX_.data(), MaxDisplay),
                              cy->ReadLast(bufY_.data(), MaxDisplay));
            if (n) {
                std::vector<double> xs(n), ys(n);
                for (size_t i = 0; i < n; ++i) { xs[i] = bufX_[i].value; ys[i] = bufY_[i].value; }
                ImPlot::PlotScatter("data", xs.data(), ys.data(), (int)n);
            }
        }
        ImPlot::EndPlot();
    }
    ImGui::End();
}

} // namespace ks::ui

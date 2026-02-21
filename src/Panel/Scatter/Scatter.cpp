#include "Scatter.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <implot.h>

namespace KiloScope {

void Scatter::OnData(Data::DataStore& store) {
    plotCount_ = 0;
    Data::Channel* chs[2]{store.GetChannel((uint16_t)chX_),
                           store.GetChannel((uint16_t)chY_)};
    Data::Sample*  bufs[2]{bufX_.data(), bufY_.data()};
    auto n = Data::ReadAligned(chs, bufs, MaxDisplay);
    if (!n) return;

    if (n > xs_.size()) { xs_.resize(n); ys_.resize(n); }
    for (size_t i = 0; i < n; ++i) {
        xs_[i] = bufX_[i].value;
        ys_[i] = bufY_[i].value;
    }
    plotCount_ = n;
}

void Scatter::OnDraw() {
    auto ids = store_->ChannelIds();
    if (ids.size() < 2) {
        ImGui::TextWrapped("Need at least 2 channels.");
        return;
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
        if (plotCount_)
            ImPlot::PlotScatter("data", xs_.data(), ys_.data(), (int)plotCount_);
        ImPlot::EndPlot();
    }
}

json Scatter::SaveSettings() const {
    return {{"chX", chX_}, {"chY", chY_}};
}

void Scatter::LoadSettings(const json& j) {
    chX_ = j.value("chX", chX_);
    chY_ = j.value("chY", chY_);
}

REGISTER_PANEL(Scatter, "Scatter", "Scatter Plot", KiloScope::PanelFlags::None)

} // namespace KiloScope

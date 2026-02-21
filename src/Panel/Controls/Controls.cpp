#include "Controls.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <cinttypes>

namespace KiloScope {

void Controls::OnData(Data::DataStore& store) {
    packets_ = store.TotalPackets();
    samples_ = store.TotalSamples();
    channels_.clear();
    for (auto id : store.ChannelIds()) {
        if (auto* ch = store.GetChannel(id))
            channels_.push_back({ch->Name(), ch->Size(), ch->MinValue(), ch->MaxValue()});
    }
}

void Controls::OnDraw() {
    ImGui::SeparatorText("Statistics");
    ImGui::Text("Packets: %" PRIu64, packets_);
    ImGui::Text("Samples: %" PRIu64, samples_);
    ImGui::Text("Channels: %zu", channels_.size());

    ImGui::SeparatorText("Channels");
    for (auto& ch : channels_)
        ImGui::BulletText("%s: %zu [%.2f, %.2f]", ch.name.c_str(), ch.size, ch.min, ch.max);

    ImGui::Separator();
    if (ImGui::Button("Clear Data")) store_->Clear();
}

REGISTER_PANEL(Controls, "Controls", "Controls",
    KiloScope::PanelFlags::Singleton | KiloScope::PanelFlags::NoSettings)

} // namespace KiloScope

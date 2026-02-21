#include "Controls.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include <imgui.h>
#include <cinttypes>

namespace KiloScope {

void Controls::OnDraw() {
    ImGui::SeparatorText("Statistics");
    ImGui::Text("Packets: %" PRIu64, store_->TotalPackets());
    ImGui::Text("Samples: %" PRIu64, store_->TotalSamples());

    auto ids = store_->ChannelIds();
    ImGui::Text("Channels: %zu", ids.size());

    ImGui::SeparatorText("Channels");
    {
        std::shared_lock lk(store_->Mutex());
        for (auto id : ids) {
            if (auto* ch = store_->GetChannel(id))
                ImGui::BulletText("%s: %zu [%.2f, %.2f]",
                    ch->Name().c_str(), ch->Size(), ch->MinValue(), ch->MaxValue());
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Data")) store_->Clear();
}

REGISTER_PANEL(Controls, "Controls", "Controls",
    KiloScope::PanelFlags::Singleton | KiloScope::PanelFlags::NoSettings)

} // namespace KiloScope

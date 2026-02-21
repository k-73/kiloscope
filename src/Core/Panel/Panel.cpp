#include "Core/Panel/Panel.hpp"
#include <imgui.h>
#include <unordered_map>

namespace KiloScope {

Panel::Panel(std::string_view typeId, std::string title, PanelFlags flags)
    : typeId_(typeId)
    , title_(std::move(title))
    , flags_(flags)
{
    int inst = NextInstanceId(typeId_);
    id_ = typeId_ + "##" + std::to_string(inst);
}

void Panel::Draw() {
    if (!visible_) return;
    bool open = true;
    ImGui::Begin(id_.c_str(), &open);
    if (!open) { visible_ = false; ImGui::End(); return; }
    OnDraw();
    ImGui::End();
}

int Panel::NextInstanceId(const std::string& typeId) {
    static std::unordered_map<std::string, int> counters;
    return counters[typeId]++;
}

} // namespace KiloScope

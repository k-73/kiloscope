#include "Core/Panel/Panel.hpp"
#include <chrono>
#include <imgui.h>
#include <mutex>
#include <unordered_map>

namespace Kilo {

Panel::Panel(std::string_view typeId, std::string title, PanelFlags flags)
    : typeId_(typeId)
    , title_(std::move(title))
    , flags_(flags)
{
    int inst = NextInstanceId(typeId_);
    id_ = typeId_ + "##" + std::to_string(inst);
}

Panel::~Panel() = default;

void Panel::Draw() {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    std::lock_guard g(mutex_);
    auto t1 = Clock::now();

    timing_.mutexWaitUs = std::chrono::duration<float, std::micro>(t1 - t0).count();

    if (!visible_) return;
    bool open = true;
    ImGui::Begin(id_.c_str(), &open);
    if (!open) { visible_ = false; ImGui::End(); return; }
    OnDraw();
    ImGui::End();

    timing_.drawUs = std::chrono::duration<float, std::micro>(Clock::now() - t1).count();
}

int Panel::NextInstanceId(const std::string& typeId) {
    static std::mutex mutex;
    static std::unordered_map<std::string, int> counters;
    std::lock_guard lock(mutex);
    return counters[typeId]++;
}

} // namespace Kilo

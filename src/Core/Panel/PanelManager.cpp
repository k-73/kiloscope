#include "Core/Panel/PanelManager.hpp"
#include "Core/Log.hpp"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <fstream>

namespace Kilo {

PanelManager::PanelManager() = default;

PanelManager::~PanelManager() {
    worker_.request_stop();
    if (worker_.joinable()) worker_.join();
}

// ── lifecycle ───────────────────────────────────────────────────

void PanelManager::Start() {
    worker_ = std::jthread([this](std::stop_token st) { WorkerLoop(st); });
}

Panel* PanelManager::Add(std::string_view typeId) {
    std::unique_lock lock(panelsMutex_);

    for (auto& p : panels_)
        if (p->TypeId() == typeId) { p->SetVisible(true); return p.get(); }

    auto panel = PanelRegistry::Instance().Create(typeId);
    if (!panel) return nullptr;

    panel->OnAttach();
    auto* ptr = panel.get();
    panels_.push_back(std::move(panel));
    Log::UI().info("Panel added: {} ({})", ptr->Id(), ptr->Title());
    return ptr;
}

void PanelManager::Remove(std::string_view id) {
    std::unique_lock lock(panelsMutex_);

    auto it = std::find_if(panels_.begin(), panels_.end(),
        [&](auto& p) { return p->Id() == id; });
    if (it == panels_.end()) return;

    (*it)->OnDetach();
    Log::UI().info("Panel removed: {}", id);
    panels_.erase(it);
}

bool PanelManager::Empty() const {
    std::shared_lock lock(panelsMutex_);
    return panels_.empty();
}

// ── main thread ─────────────────────────────────────────────────

void PanelManager::Draw() {
    std::shared_lock lock(panelsMutex_);
    for (auto& p : panels_) p->Draw();

    // Update timing snapshot (main thread only — no lock needed for snapshot)
    auto& snap = PanelTimingSnapshot::Get();
    snap.panels.resize(panels_.size());
    for (size_t i = 0; i < panels_.size(); ++i) {
        auto& t = panels_[i]->timing_;
        auto& e = snap.panels[i];
        e.title       = panels_[i]->Title().c_str();
        e.drawUs      = t.drawUs;
        e.loopUs      = t.loopUs;
        e.drawPeak    = std::max(e.drawPeak, t.drawUs);
        e.loopPeak    = std::max(e.loopPeak, t.loopUs);
        e.mutexWaitUs = t.mutexWaitUs;
        e.visible     = panels_[i]->IsVisible();
    }
}

void PanelManager::DrawMenuBar() {
    std::shared_lock lock(panelsMutex_);
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Panels")) {
        for (auto& p : panels_) {
            ImGui::PushID(p->Id().c_str());
            bool vis = p->IsVisible();
            if (ImGui::MenuItem(p->Title().c_str(), nullptr, vis))
                p->SetVisible(!vis);
            ImGui::PopID();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

// ── worker thread ───────────────────────────────────────────────

void PanelManager::WorkerLoop(std::stop_token st) {
    using Clock = std::chrono::steady_clock;
    constexpr auto kInterval = std::chrono::milliseconds(1);
    auto next = Clock::now() + kInterval;

    while (!st.stop_requested()) {
        auto cycleStart = Clock::now();
        {
            std::shared_lock lock(panelsMutex_);
            for (auto& p : panels_) {
                std::lock_guard g(p->mutex_);
                auto t = Clock::now();
                p->OnLoop();
                p->timing_.loopUs = std::chrono::duration<float, std::micro>(Clock::now() - t).count();
            }
        }
        float cycleUs = std::chrono::duration<float, std::micro>(Clock::now() - cycleStart).count();
        auto& snap = PanelTimingSnapshot::Get();
        // EMA smoothing (α=0.05 ≈ ~20 tick window at 1kHz)
        snap.workerCycleUs += 0.05f * (cycleUs - snap.workerCycleUs);
        snap.workerPeakUs  = std::max(snap.workerPeakUs, cycleUs);
        if (cycleUs > 1000.f) ++snap.workerOverruns;

        // Fixed-rate timing: sleep until next tick, clamp to now if overrun
        auto now = Clock::now();
        if (next < now) next = now;
        std::this_thread::sleep_until(next);
        next += kInterval;
    }
}

// ── persistence ─────────────────────────────────────────────────

void PanelManager::SaveToFile(const std::string& path) const {
    json arr = json::array();
    {
        std::shared_lock lock(panelsMutex_);
        for (auto& p : panels_) {
            json entry;
            entry["typeId"]  = p->TypeId();
            entry["visible"] = p->IsVisible();
            if (!(p->Flags() & PanelFlags::NoSettings)) {
                auto settings = p->SaveSettings();
                if (!settings.is_null() && !settings.empty())
                    entry["settings"] = settings;
            }
            arr.push_back(entry);
        }
    }

    std::ofstream f(path);
    if (f) {
        f << arr.dump(2);
        Log::UI().info("Panels saved to {}", path);
    } else {
        Log::UI().error("Failed to save panels to {}", path);
    }
}

void PanelManager::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        Log::UI().warn("No panel config found at {}, using defaults", path);
        return;
    }
    try {
        json arr = json::parse(f);
        for (auto& entry : arr) {
            auto* panel = Add(entry.at("typeId").get<std::string>());
            if (!panel) continue;
            panel->SetVisible(entry.value("visible", true));
            if (entry.contains("settings"))
                panel->LoadSettings(entry["settings"]);
        }
        Log::UI().info("Panels loaded from {}", path);
    } catch (const std::exception& e) {
        Log::UI().error("Failed to load panels from {}: {}", path, e.what());
    }
}

} // namespace Kilo

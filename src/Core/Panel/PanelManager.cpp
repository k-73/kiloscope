#include "Core/Panel/PanelManager.hpp"
#include "Core/Log.hpp"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <fstream>

namespace KiloScope {

PanelManager::PanelManager(std::shared_ptr<Data::DataStore> store)
    : store_(std::move(store))
    , worker_([this](std::stop_token st) { WorkerLoop(st); })
{}

PanelManager::~PanelManager() {
    worker_.request_stop();
    if (worker_.joinable()) worker_.join();
}

Panel* PanelManager::Add(std::string_view typeId) {
    // One instance per type — return existing if present
    for (auto& p : panels_) {
        if (p->TypeId() == typeId) {
            p->SetVisible(true);
            return p.get();
        }
    }

    auto panel = PanelRegistry::Instance().Create(typeId);
    if (!panel) return nullptr;
    panel->BindStore(store_);
    panel->OnAttach();
    auto* ptr = panel.get();
    panels_.push_back(std::move(panel));
    Log::UI().info("Panel added: {} ({})", ptr->Id(), ptr->Title());
    return ptr;
}

void PanelManager::Remove(const std::string& id) {
    auto it = std::find_if(panels_.begin(), panels_.end(),
        [&](auto& p) { return p->Id() == id; });
    if (it != panels_.end()) {
        (*it)->OnDetach();
        Log::UI().info("Panel removed: {}", id);
        panels_.erase(it);
    }
}

void PanelManager::NotifyData() {
    for (auto& p : panels_) p->MarkDirty();
}

void PanelManager::Draw() {
    for (auto& p : panels_) p->Draw();
}

void PanelManager::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
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
}

void PanelManager::WorkerLoop(std::stop_token st) {
    using namespace std::chrono_literals;
    while (!st.stop_requested()) {
        {
            std::shared_lock lk(store_->Mutex());
            for (auto& p : panels_) {
                std::lock_guard g(p->mutex_);
                if (p->dirty_.exchange(false, std::memory_order_relaxed))
                    p->OnData(*store_);
                p->OnLoop();
            }
        }
        std::this_thread::sleep_for(1ms);
    }
}

void PanelManager::SaveToFile(const std::string& path) const {
    json arr = json::array();
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
            auto typeId = entry.at("typeId").get<std::string>();
            auto* panel = Add(typeId);
            if (!panel) continue;
            if (entry.contains("visible"))
                panel->SetVisible(entry["visible"].get<bool>());
            if (entry.contains("settings"))
                panel->LoadSettings(entry["settings"]);
        }
        Log::UI().info("Panels loaded from {}", path);
    } catch (const std::exception& e) {
        Log::UI().error("Failed to load panels from {}: {}", path, e.what());
    }
}

} // namespace KiloScope

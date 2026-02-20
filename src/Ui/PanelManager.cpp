#include "PanelManager.hpp"
#include "Core/Log.hpp"
#include <imgui.h>
#include <algorithm>
#include <fstream>

namespace KiloScope::UI {

PanelManager::PanelManager(std::shared_ptr<Data::DataStore> store)
    : store_(std::move(store)) {}

Panel* PanelManager::Add(std::string_view typeId) {
    // Enforce singleton constraint
    auto& reg = PanelRegistry::Instance();
    for (auto& e : reg.Entries()) {
        if (e.typeId == typeId && (e.defaultFlags & PanelFlags::Singleton)) {
            for (auto& p : panels_) {
                if (p->TypeId() == typeId) {
                    p->SetVisible(true);
                    return p.get();
                }
            }
        }
    }

    auto panel = reg.Create(typeId, store_);
    if (!panel) return nullptr;
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

void PanelManager::Update() {
    for (auto& p : panels_) {
        if (p->IsVisible()) p->OnUpdate();
    }
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
            ImGui::Separator();
            auto& reg = PanelRegistry::Instance();
            if (ImGui::BeginMenu("Add Panel")) {
                for (auto& e : reg.Entries()) {
                    if (ImGui::MenuItem(e.displayName.c_str()))
                        Add(e.typeId);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
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

} // namespace KiloScope::UI

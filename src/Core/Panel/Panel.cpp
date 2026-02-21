#include "Core/Panel/Panel.hpp"
#include "Render/Scene.hpp"
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

Panel::~Panel() = default;

void Panel::BindScene(std::unique_ptr<Render::Scene> scene) {
    scene_ = std::move(scene);
}

void Panel::Draw() {
    if (!visible_) return;
    std::lock_guard g(mutex_);
    bool open = true;
    ImGui::Begin(id_.c_str(), &open);
    if (!open) { visible_ = false; ImGui::End(); return; }
    OnDraw();
    ImGui::End();
}

void Panel::DrawViewport() {
    if (!scene_) return;

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(avail.x));
    int h = std::max(1, static_cast<int>(avail.y));
    scene_->Resize(w, h);

    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##vp", avail,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    auto& io  = ImGui::GetIO();
    auto& cam = scene_->GetCamera();

    if (ImGui::IsItemHovered() && io.MouseWheel != 0)
        cam.Zoom(io.MouseWheel);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    scene_->BeginRender();
    OnRender(*scene_);
    scene_->EndRender();

    ImGui::SetCursorScreenPos(cursor);
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(scene_->Texture())),
                 avail, {0, 1}, {1, 0});
}

int Panel::NextInstanceId(const std::string& typeId) {
    static std::unordered_map<std::string, int> counters;
    return counters[typeId]++;
}

} // namespace KiloScope

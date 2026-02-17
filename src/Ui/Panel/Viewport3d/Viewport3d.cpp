#include "Viewport3d.hpp"
#include <imgui.h>

#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

namespace KiloScope::UI {

void Viewport3d::Draw() {
    ImGui::Begin(title_.c_str(), &visible_);

    if (!init_) {
        scene_->Init(std::string(ASSETS_DIR) + "/shaders");
        init_ = true;
    }

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, (int)avail.x), h = std::max(1, (int)avail.y);
    scene_->Resize(w, h);

    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##vp", avail,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    auto& io = ImGui::GetIO();
    auto& cam = scene_->GetCamera();

    if (ImGui::IsItemHovered() && io.MouseWheel != 0) cam.Zoom(io.MouseWheel);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    GLuint tex = scene_->Render(store_);
    ImGui::SetCursorScreenPos(cursor);
    ImGui::Image((ImTextureID)(uintptr_t)tex, avail, {0, 1}, {1, 0});
    ImGui::End();
}

} // namespace KiloScope::UI

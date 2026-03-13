#include "Core/Panel/Panel.hpp"
#include "Render/Scene.hpp"
#include "Render/Draw.hpp"
#include <imgui.h>
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
    if (!visible_) return;
    std::lock_guard g(mutex_);
    bool open = true;
    ImGui::Begin(id_.c_str(), &open);
    if (!open) { visible_ = false; ImGui::End(); return; }
    OnDraw();
    ImGui::End();
}

void Panel::RenderBegin(const char* name, const ViewportConfig& cfg) {
    auto& scene = scenes_[name];
    if (!scene) {
        scene = std::make_unique<Render::Scene>();
        scene->Init(shaderDir_);
    }

    auto avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, static_cast<int>(cfg.width  > 0 ? cfg.width  : avail.x));
    int h = std::max(1, static_cast<int>(cfg.height > 0 ? cfg.height : avail.y));
    scene->Resize(w, h);

    ImVec2 size{static_cast<float>(w), static_cast<float>(h)};
    auto cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(name, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    auto& io  = ImGui::GetIO();
    auto& cam = scene->GetCamera();

    if (ImGui::IsItemHovered() && io.MouseWheel != 0)
        cam.Zoom(io.MouseWheel);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        cam.Orbit(io.MouseDelta.x, io.MouseDelta.y);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        cam.Pan(io.MouseDelta.x, io.MouseDelta.y);

    sf_ = { scene.get(), cursor.x, cursor.y, size.x, size.y };
    scene->BeginRender();
    Render::SetContext(&scene->Prims(), &cam);
}

void Panel::RenderEnd() {
    Render::SetContext(nullptr, &sf_.scene->GetCamera());
    sf_.scene->EndRender();
    ImGui::SetCursorScreenPos({sf_.cursorX, sf_.cursorY});
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(sf_.scene->Texture())),
                 {sf_.w, sf_.h}, {0, 1}, {1, 0});
}

int Panel::NextInstanceId(const std::string& typeId) {
    static std::unordered_map<std::string, int> counters;
    return counters[typeId]++;
}

} // namespace Kilo

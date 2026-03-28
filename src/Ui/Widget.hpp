#pragma once
#include <glm/glm.hpp>
#include <imgui.h>
#include <algorithm>

namespace Kilo::Widget {

// 2D joystick pad. Returns true while held; writes normalized (-1..1) displacement to *out.
inline bool Joystick(const char* id, glm::vec2* out, float size = 80.f) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float half = size * 0.5f;
    ImVec2 c(p.x + half, p.y + half);

    ImGui::InvisibleButton(id, {size, size});
    bool held = ImGui::IsItemActive();

    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + size, p.y + size}, IM_COL32(25, 30, 40, 255), 4.f);
    dl->AddRect(p, {p.x + size, p.y + size}, IM_COL32(60, 70, 90, 200), 4.f);
    dl->AddLine({c.x, p.y + 2}, {c.x, p.y + size - 2}, IM_COL32(50, 55, 70, 180));
    dl->AddLine({p.x + 2, c.y}, {p.x + size - 2, c.y}, IM_COL32(50, 55, 70, 180));

    glm::vec2 d(0.f);
    if (held) {
        auto m = ImGui::GetMousePos();
        d.x = std::clamp((m.x - c.x) / (half - 6.f), -1.f, 1.f);
        d.y = std::clamp((m.y - c.y) / (half - 6.f), -1.f, 1.f);
    }

    ImVec2 knob(c.x + d.x * (half - 6.f), c.y + d.y * (half - 6.f));
    dl->AddCircleFilled(knob, 5.f, held ? IM_COL32(144, 176, 208, 255) : IM_COL32(90, 110, 140, 255));
    dl->AddCircle(knob, 5.f, IM_COL32(160, 185, 215, 220));

    if (out) *out = d;
    return held;
}

} // namespace Kilo::Widget

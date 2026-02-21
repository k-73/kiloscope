#include "Viewport3d.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Scene.hpp"

namespace KiloScope {

Viewport3d::Viewport3d()
    : Panel("Viewport3d", "3D Viewport",
            PanelFlags::Singleton | PanelFlags::NeedsScene)
{
    for (auto& b : sampleBufs_) b.resize(MaxPts);
    path_.reserve(MaxPts);
}

void Viewport3d::OnData(Data::DataStore& store) {
    size_t counts[3]{};
    for (int i = 0; i < 3; ++i)
        if (auto* ch = store.GetChannel(i))
            counts[i] = ch->ReadLast(sampleBufs_[i].data(), MaxPts);

    auto n = std::max({counts[0], counts[1], counts[2]});
    if (!n) { hasData_ = false; return; }

    auto val = [&](int ch, size_t idx) {
        return idx < counts[ch] ? static_cast<float>(sampleBufs_[ch][idx].value) : 0.f;
    };

    path_.resize(n);
    for (size_t i = 0; i < n; ++i)
        path_[i] = {val(0, i), val(1, i), val(2, i)};

    size_t last[3]{};
    for (int i = 0; i < 3; ++i) last[i] = counts[i] ? counts[i] - 1 : 0;
    endpoint_ = {val(0, last[0]), val(1, last[1]), val(2, last[2])};
    hasData_ = true;
}

void Viewport3d::OnRender(Render::Scene& scene) {
    auto& p = scene.Prims();
    p.DrawAxes({0, 0, 0}, 1.f);

    if (!hasData_) return;

    for (size_t i = 1; i < path_.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(path_.size());
        p.DrawLine(path_[i - 1], path_[i],
                   {.2f + .8f * t, .4f, 1.f - .6f * t, 1.f}, 2.5f);
    }

    p.DrawSphere(endpoint_, 0.1f, {1.f, .8f, .2f, 1.f}, 32);
}

void Viewport3d::OnDraw() {
    DrawViewport();
}

REGISTER_PANEL(Viewport3d, "Viewport3d", "3D Viewport",
    KiloScope::PanelFlags::Singleton | KiloScope::PanelFlags::NeedsScene)

} // namespace KiloScope

#include "Viewport3d.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Scene.hpp"

namespace KiloScope {

Viewport3d::Viewport3d()
    : Panel("Viewport3d", "3D Viewport",
            PanelFlags::Singleton | PanelFlags::NeedsScene)
{
    for (auto& b : sampleBufs_) b.resize(MaxPts);
    pending_.path.reserve(MaxPts);
    active_.path.reserve(MaxPts);
}

void Viewport3d::OnData(Data::DataStore& store) {
    // Snapshot write positions first (3 atomic loads, near-instantaneous).
    // This ensures all channels are read up to the same logical time,
    // eliminating cross-channel misalignment from concurrent Ingest.
    Data::Channel* chs[3]{};
    size_t wpos[3]{};
    for (int i = 0; i < 3; ++i)
        if ((chs[i] = store.GetChannel(i)))
            wpos[i] = chs[i]->WritePos();

    size_t endPos = SIZE_MAX;
    for (int i = 0; i < 3; ++i)
        if (chs[i]) endPos = std::min(endPos, wpos[i]);

    if (endPos == 0 || endPos == SIZE_MAX) {
        pending_.valid = false; newData_ = true; return;
    }

    size_t n = 0;
    for (int i = 0; i < 3; ++i)
        if (chs[i]) n = chs[i]->ReadAt(sampleBufs_[i].data(), MaxPts, endPos);

    if (!n) { pending_.valid = false; newData_ = true; return; }

    pending_.path.resize(n);
    for (size_t i = 0; i < n; ++i)
        pending_.path[i] = {
            static_cast<float>(sampleBufs_[0][i].value),
            static_cast<float>(sampleBufs_[1][i].value),
            static_cast<float>(sampleBufs_[2][i].value)};

    pending_.endpoint = pending_.path[n - 1];
    pending_.valid = true;
    newData_ = true;
}

void Viewport3d::OnRender(Render::Scene& scene) {
    if (newData_) {
        std::swap(active_, pending_);
        newData_ = false;
    }

    auto& p = scene.Prims();
    p.DrawAxes({0, 0, 0}, 1.f);

    if (!active_.valid) return;

    for (size_t i = 1; i < active_.path.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(active_.path.size());
        p.DrawLine(active_.path[i - 1], active_.path[i],
                   {.2f + .8f * t, .4f, 1.f - .6f * t, 1.f}, 2.5f);
    }

    p.DrawSphere(active_.endpoint, 0.1f, {1.f, .8f, .2f, 1.f}, 32);
}

void Viewport3d::OnDraw() {
    DrawViewport();
}

REGISTER_PANEL(Viewport3d, "Viewport3d", "3D Viewport",
    KiloScope::PanelFlags::Singleton | KiloScope::PanelFlags::NeedsScene)

} // namespace KiloScope

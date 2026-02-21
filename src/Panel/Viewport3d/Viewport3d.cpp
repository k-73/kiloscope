#include "Viewport3d.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Scene.hpp"

namespace KiloScope {

void Viewport3d::OnData(Data::DataStore& store) {
    std::vector<Data::Sample> bufs[3];
    size_t counts[3]{};

    for (int i = 0; i < 3; ++i) {
        bufs[i].resize(MaxPts);
        if (auto* ch = store.GetChannel(i))
            counts[i] = ch->ReadLast(bufs[i].data(), MaxPts);
    }

    auto n = std::max({counts[0], counts[1], counts[2]});
    if (!n) { cache_.empty = true; return; }

    auto val = [&](int ch, size_t idx) {
        return idx < counts[ch] ? static_cast<float>(bufs[ch][idx].value) : 0.f;
    };

    cache_.path.resize(n);
    for (size_t i = 0; i < n; ++i)
        cache_.path[i] = {val(0, i), val(1, i), val(2, i)};

    size_t last[3]{};
    for (int i = 0; i < 3; ++i) last[i] = counts[i] ? counts[i] - 1 : 0;
    cache_.endpoint = {val(0, last[0]), val(1, last[1]), val(2, last[2])};
    cache_.empty = false;
}

void Viewport3d::OnRender(Render::Scene& scene) {
    auto& p = scene.Prims();
    p.DrawAxes({0, 0, 0}, 1.f);

    if (cache_.empty) return;

    for (size_t i = 1; i < cache_.path.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(cache_.path.size());
        p.DrawLine(cache_.path[i - 1], cache_.path[i],
                   {.2f + .8f * t, .4f, 1.f - .6f * t, 1.f}, 2.5f);
    }

    p.DrawSphere(cache_.endpoint, 0.1f, {1.f, .8f, .2f, 1.f}, 32);
}

void Viewport3d::OnDraw() {
    DrawViewport();
}

REGISTER_PANEL(Viewport3d, "Viewport3d", "3D Viewport",
    KiloScope::PanelFlags::Singleton | KiloScope::PanelFlags::NeedsScene)

} // namespace KiloScope

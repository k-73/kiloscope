#pragma once
#include "Primitives.hpp"
#include "Data/DataStore.hpp"
#include <memory>

namespace KiloScope::Render {

class GeometryRenderer {
public:
    void Init(Primitives& p) { prims_ = &p; bufs_.resize(3); for (auto& b : bufs_) b.resize(MaxPts); }

    void Draw(std::shared_ptr<Data::DataStore> store) {
        if (!prims_ || !store) return;
        std::shared_lock lk(store->Mutex());

        size_t counts[3]{};
        for (int i = 0; i < 3; ++i)
            if (auto* ch = store->GetChannel(i)) counts[i] = ch->ReadLast(bufs_[i].data(), MaxPts);

        auto n = std::max({counts[0], counts[1], counts[2]});
        if (!n) return;

        auto val = [&](int ch, size_t i) { return i < counts[ch] ? (float)bufs_[ch][i].value : 0.f; };

        glm::vec3 prev(0);
        for (size_t i = 0; i < n; ++i) {
            glm::vec3 pt(val(0,i), val(1,i), val(2,i));
            if (i > 0) {
                float t = (float)i / (float)n;
                prims_->DrawLine(prev, pt, {.2f+.8f*t, .4f, 1.f-.6f*t, 1.f}, 2.5f);
            }
            prev = pt;
        }

        // Endpoint sphere — guard against empty channels
        size_t lastIdx[3]{};
        for (int i = 0; i < 3; ++i) lastIdx[i] = counts[i] ? counts[i] - 1 : 0;
        prims_->DrawSphere({val(0,lastIdx[0]), val(1,lastIdx[1]), val(2,lastIdx[2])},
                           0.1f, {1.f, .8f, .2f, 1.f}, 32);
    }

private:
    static constexpr size_t MaxPts = 4096;
    Primitives* prims_ = nullptr;
    std::vector<std::vector<Data::Sample>> bufs_;
};

} // namespace KiloScope::Render

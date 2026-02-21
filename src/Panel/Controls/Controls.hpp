#pragma once
#include "Core/Panel/Panel.hpp"
#include <vector>

namespace KiloScope {

class Controls : public Panel {
public:
    Controls()
        : Panel("Controls", "Controls", PanelFlags::Singleton | PanelFlags::NoSettings) {}

    void OnData(Data::DataStore& store) override;
    void OnDraw() override;

private:
    struct ChannelInfo { std::string name; size_t size; double min, max; };
    std::vector<ChannelInfo> channels_;
    uint64_t packets_ = 0, samples_ = 0;
};

} // namespace KiloScope

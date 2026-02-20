#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Diagnostics : public Panel {
public:
    explicit Diagnostics(std::shared_ptr<Data::DataStore> s)
        : Panel("Diagnostics", "Diagnostics", std::move(s),
                PanelFlags::Singleton | PanelFlags::NoSettings) {}
    void OnDraw() override;

private:
    bool showImGuiDemo_  = false;
    bool showImPlotDemo_ = false;
    float frameTimes_[128] = {};
    int frameIdx_ = 0;
};

} // namespace KiloScope::UI

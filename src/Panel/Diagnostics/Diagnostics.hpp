#pragma once
#include "Core/Panel/Panel.hpp"

namespace Kilo {

class Diagnostics : public Panel {
public:
    Diagnostics()
        : Panel("Diagnostics", "Diagnostics",
                PanelFlags::Singleton | PanelFlags::NoSettings) {}
    void OnDraw() override;

private:
    bool showImGuiDemo_  = false;
    bool showImPlotDemo_ = false;
    float frameTimes_[128] = {};
    int frameIdx_ = 0;
};

} // namespace Kilo

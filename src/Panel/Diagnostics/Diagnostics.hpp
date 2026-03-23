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

    static constexpr int kHistory = 128;
    float fpsHistory_[kHistory] = {};
    int   histIdx_    = 0;
    int   sampleSkip_ = 0;
};

} // namespace Kilo

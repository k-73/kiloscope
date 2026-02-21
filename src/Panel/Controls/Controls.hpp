#pragma once
#include "Core/Panel/Panel.hpp"

namespace KiloScope {

class Controls : public Panel {
public:
    Controls()
        : Panel("Controls", "Controls", PanelFlags::Singleton | PanelFlags::NoSettings) {}
    void OnDraw() override;
};

} // namespace KiloScope

#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Controls : public Panel {
public:
    explicit Controls(std::shared_ptr<Data::DataStore> s)
        : Panel("Controls", "Controls", std::move(s), PanelFlags::Singleton | PanelFlags::NoSettings) {}
    void OnDraw() override;
};

} // namespace KiloScope::UI

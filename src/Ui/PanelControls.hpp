#pragma once
#include "Panel.hpp"

namespace ks::ui {

class PanelControls : public Panel {
public:
    using Panel::Panel;
    explicit PanelControls(std::shared_ptr<data::DataStore> s) : Panel("Controls", std::move(s)) {}
    void Draw() override;
};

} // namespace ks::ui

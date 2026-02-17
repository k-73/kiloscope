#pragma once
#include "../Panel.hpp"

namespace ks::ui {

class Controls : public Panel {
public:
    explicit Controls(std::shared_ptr<data::DataStore> s) : Panel("Controls", std::move(s)) {}
    void Draw() override;
};

} // namespace ks::ui

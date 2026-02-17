#pragma once
#include "Panel.hpp"
#include <memory>
#include <vector>

namespace ks::ui {

class Layout {
public:
    void Add(std::unique_ptr<Panel> p) { panels_.push_back(std::move(p)); }
    void Draw() { for (auto& p : panels_) if (p->IsVisible()) p->Draw(); }

private:
    std::vector<std::unique_ptr<Panel>> panels_;
};

} // namespace ks::ui

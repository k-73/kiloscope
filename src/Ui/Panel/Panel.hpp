#pragma once
#include "Data/DataStore.hpp"
#include <memory>
#include <string>

namespace KiloScope::UI {

class Panel {
public:
    Panel(std::string title, std::shared_ptr<Data::DataStore> s)
        : title_(std::move(title)), store_(std::move(s)) {}
    virtual ~Panel() = default;
    virtual void Draw() = 0;
    const std::string& Title() const { return title_; }
    bool IsVisible() const { return visible_; }

protected:
    std::string title_;
    std::shared_ptr<Data::DataStore> store_;
    bool visible_ = true;
};

} // namespace KiloScope::UI

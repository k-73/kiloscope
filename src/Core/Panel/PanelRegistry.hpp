#pragma once
#include "Panel.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Kilo {

struct PanelEntry {
    std::string typeId;
    std::string displayName;
    std::function<std::unique_ptr<Panel>()> factory;
};

class PanelRegistry {
public:
    static PanelRegistry& Instance();

    void Register(PanelEntry entry);
    std::unique_ptr<Panel> Create(std::string_view typeId) const;
    const std::vector<PanelEntry>& Entries() const { return entries_; }

private:
    PanelRegistry() = default;
    std::vector<PanelEntry> entries_;
};

template<typename T>
bool RegisterPanel(std::string typeId, std::string displayName) {
    PanelRegistry::Instance().Register({
        std::move(typeId), std::move(displayName),
        []() -> std::unique_ptr<Panel> { return std::make_unique<T>(); }
    });
    return true;
}

} // namespace Kilo

#pragma once
#include "Panel.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace KiloScope {

struct PanelEntry {
    std::string typeId;
    std::string displayName;
    PanelFlags  defaultFlags;
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

// Helper for auto-registration
struct PanelRegistrar {
    PanelRegistrar(PanelEntry entry) { PanelRegistry::Instance().Register(std::move(entry)); }
};

} // namespace KiloScope

#define REGISTER_PANEL(Class, typeId, displayName, flags)                        \
    static ::KiloScope::PanelRegistrar sReg_##Class({                           \
        typeId, displayName, flags,                                              \
        []() -> std::unique_ptr<::KiloScope::Panel> {                           \
            return std::make_unique<Class>();                                     \
        }                                                                        \
    });

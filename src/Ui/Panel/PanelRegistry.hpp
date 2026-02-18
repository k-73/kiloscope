#pragma once
#include "Panel.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace KiloScope::UI {

struct PanelEntry {
    std::string typeId;
    std::string displayName;
    PanelFlags  defaultFlags;
    std::function<std::unique_ptr<Panel>(std::shared_ptr<Data::DataStore>)> factory;
};

class PanelRegistry {
public:
    static PanelRegistry& Instance();

    void Register(PanelEntry entry);
    std::unique_ptr<Panel> Create(std::string_view typeId, std::shared_ptr<Data::DataStore> store) const;
    const std::vector<PanelEntry>& Entries() const { return entries_; }

private:
    PanelRegistry() = default;
    std::vector<PanelEntry> entries_;
};

// Helper for auto-registration
struct PanelRegistrar {
    PanelRegistrar(PanelEntry entry) { PanelRegistry::Instance().Register(std::move(entry)); }
};

} // namespace KiloScope::UI

#define REGISTER_PANEL(Class, typeId, displayName, flags)                        \
    static ::KiloScope::UI::PanelRegistrar sReg_##Class({                       \
        typeId, displayName, flags,                                              \
        [](std::shared_ptr<::KiloScope::Data::DataStore> s)                     \
            -> std::unique_ptr<::KiloScope::UI::Panel> {                        \
            return std::make_unique<Class>(std::move(s));                        \
        }                                                                        \
    });

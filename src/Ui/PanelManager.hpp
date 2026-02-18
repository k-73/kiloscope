#pragma once
#include "Panel/Panel.hpp"
#include "Panel/PanelRegistry.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace KiloScope::UI {

class PanelManager {
public:
    explicit PanelManager(std::shared_ptr<Data::DataStore> store);

    Panel* Add(std::string_view typeId);
    void   Remove(const std::string& id);
    void   Update();
    void   Draw();
    void   DrawMenuBar();

    bool Empty() const { return panels_.empty(); }

    void SaveToFile(const std::string& path) const;
    void LoadFromFile(const std::string& path);

private:
    std::shared_ptr<Data::DataStore> store_;
    std::vector<std::unique_ptr<Panel>> panels_;
};

} // namespace KiloScope::UI

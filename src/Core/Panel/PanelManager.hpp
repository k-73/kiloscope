#pragma once
#include "Panel.hpp"
#include "PanelRegistry.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace KiloScope {

class PanelManager {
public:
    explicit PanelManager(std::shared_ptr<Data::DataStore> store);
    ~PanelManager();

    Panel* Add(std::string_view typeId);
    void   Remove(const std::string& id);
    void   NotifyData();
    void   Draw();
    void   DrawMenuBar();

    bool Empty() const { return panels_.empty(); }

    void SaveToFile(const std::string& path) const;
    void LoadFromFile(const std::string& path);

private:
    void WorkerLoop(std::stop_token st);

    std::shared_ptr<Data::DataStore> store_;
    std::vector<std::unique_ptr<Panel>> panels_;
    std::jthread worker_;
};

} // namespace KiloScope

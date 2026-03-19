#pragma once
#include "Panel.hpp"
#include "PanelRegistry.hpp"
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Kilo {

class PanelManager {
public:
    PanelManager();
    ~PanelManager();

    void   Start();
    Panel* Add(std::string_view typeId);
    void   Remove(std::string_view id);
    void   Draw();
    void   DrawMenuBar();

    bool Empty() const { return panels_.empty(); }

    void SaveToFile(const std::string& path) const;
    void LoadFromFile(const std::string& path);

private:
    void WorkerLoop(std::stop_token st);

    std::vector<std::unique_ptr<Panel>> panels_;
    mutable std::shared_mutex           panelsMutex_;
    std::jthread                        worker_;
};

} // namespace Kilo

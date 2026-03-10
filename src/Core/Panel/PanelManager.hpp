#pragma once
#include "Panel.hpp"
#include "PanelRegistry.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Kilo {

class PanelManager {
public:
    explicit PanelManager(std::string shaderDir);
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

    std::string shaderDir_;
    std::vector<std::unique_ptr<Panel>> panels_;
    std::jthread worker_;
};

} // namespace Kilo

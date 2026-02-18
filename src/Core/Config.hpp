#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace KiloScope {

struct AppConfig {
    uint16_t    udpPort         = 9000;
    std::string panelConfigPath = "panels.json";

    void LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

} // namespace KiloScope

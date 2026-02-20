#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace KiloScope {

struct AppConfig {
    uint16_t    udpPort         = 9000;
    std::string panelConfigPath = "panels.json";

    // Window state
    int         windowWidth     = 1920;
    int         windowHeight    = 1080;
    int         windowPosX      = -1;   // -1 = auto/center
    int         windowPosY      = -1;
    bool        maximized       = true;
    bool        decorated       = true;

    // System
    bool        vsync           = true;
    std::string logLevel        = "info";

    void LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

} // namespace KiloScope

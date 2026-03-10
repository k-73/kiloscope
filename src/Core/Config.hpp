#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace Kilo {

struct AppConfig {
    std::string panelConfigPath = "panels.json";

    int         windowWidth     = 1920;
    int         windowHeight    = 1080;
    int         windowPosX      = -1;
    int         windowPosY      = -1;
    bool        maximized       = true;
    bool        decorated       = true;

    bool        vsync           = true;
    std::string logLevel        = "info";

    void LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

void to_json(nlohmann::json& j, const AppConfig& c);
void from_json(const nlohmann::json& j, AppConfig& c);

} // namespace Kilo

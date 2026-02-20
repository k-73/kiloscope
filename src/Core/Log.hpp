#pragma once
#include <spdlog/spdlog.h>
#include <memory>

namespace KiloScope {

class Log {
public:
    static void Init();
    static void SetLevel(const std::string& level);

    static spdlog::logger& App()    { return *sApp_; }
    static spdlog::logger& Net()    { return *sNet_; }
    static spdlog::logger& Data()   { return *sData_; }
    static spdlog::logger& Render() { return *sRender_; }
    static spdlog::logger& UI()     { return *sUI_; }

private:
    static std::shared_ptr<spdlog::logger> sApp_;
    static std::shared_ptr<spdlog::logger> sNet_;
    static std::shared_ptr<spdlog::logger> sData_;
    static std::shared_ptr<spdlog::logger> sRender_;
    static std::shared_ptr<spdlog::logger> sUI_;
};

} // namespace KiloScope

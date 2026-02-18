#include "Log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace KiloScope {

std::shared_ptr<spdlog::logger> Log::sApp_;
std::shared_ptr<spdlog::logger> Log::sNet_;
std::shared_ptr<spdlog::logger> Log::sData_;
std::shared_ptr<spdlog::logger> Log::sRender_;
std::shared_ptr<spdlog::logger> Log::sUI_;

void Log::Init() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%n] %^[%l]%$ %v");

    sApp_    = spdlog::stdout_color_mt("App");
    sNet_    = spdlog::stdout_color_mt("Net");
    sData_   = spdlog::stdout_color_mt("Data");
    sRender_ = spdlog::stdout_color_mt("Render");
    sUI_     = spdlog::stdout_color_mt("UI");

    sApp_->set_level(spdlog::level::trace);
    sNet_->set_level(spdlog::level::trace);
    sData_->set_level(spdlog::level::trace);
    sRender_->set_level(spdlog::level::trace);
    sUI_->set_level(spdlog::level::trace);
}

} // namespace KiloScope

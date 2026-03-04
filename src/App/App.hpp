#pragma once
#include "Core/Config.hpp"
#include "Ui/UiContext.hpp"
#include "Core/Panel/PanelManager.hpp"
#include <memory>

namespace KiloScope::App {

class App {
public:
    App();
    ~App();
    void Run();

private:
    void CreateDefaultPanels();

    AppConfig config_;
    std::unique_ptr<UI::UiContext> ui_;
    std::unique_ptr<PanelManager> panels_;
};

} // namespace KiloScope::App

#pragma once
#include "Core/Config.hpp"
#include "Core/Events.hpp"
#include "Net/UdpReceiver.hpp"
#include "Data/DataStore.hpp"
#include "Ui/UiContext.hpp"
#include "Ui/PanelManager.hpp"
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
    entt::dispatcher dispatcher_;
    std::shared_ptr<Data::DataStore> store_;
    std::unique_ptr<UI::UiContext> ui_;
    std::unique_ptr<UI::PanelManager> panels_;
    std::unique_ptr<Net::UdpReceiver> receiver_;
};

} // namespace KiloScope::App

#pragma once
#include "Net/UdpReceiver.hpp"
#include "Data/DataStore.hpp"
#include "Ui/UiContext.hpp"
#include "Ui/Layout.hpp"
#include <memory>

namespace ks::app {

class App {
public:
    App();
    void Run();

private:
    std::shared_ptr<data::DataStore> store_;
    std::unique_ptr<ui::UiContext> ui_;
    ui::Layout layout_;
    std::unique_ptr<net::UdpReceiver> receiver_;
};

} // namespace ks::app

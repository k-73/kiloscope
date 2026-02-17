#pragma once
#include "Net/UdpReceiver.hpp"
#include "Data/DataStore.hpp"
#include "Ui/UiContext.hpp"
#include "Ui/Layout.hpp"
#include <memory>

namespace KiloScope::App {

class App {
public:
    App();
    void Run();

private:
    std::shared_ptr<Data::DataStore> store_;
    std::unique_ptr<UI::UiContext> ui_;
    UI::Layout layout_;
    std::unique_ptr<Net::UdpReceiver> receiver_;
};

} // namespace KiloScope::App

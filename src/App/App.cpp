#include "App.hpp"
#include "Net/Packet.hpp"
#include "Ui/Panel/Controls/Controls.hpp"
#include "Ui/Panel/Timeseries/Timeseries.hpp"
#include "Ui/Panel/Scatter/Scatter.hpp"
#include "Ui/Panel/Histogram/Histogram.hpp"
#include "Ui/Panel/Viewport3d/Viewport3d.hpp"
#include <GLFW/glfw3.h>

namespace KiloScope::App {

App::App()
    : store_(std::make_shared<Data::DataStore>())
    , ui_(std::make_unique<UI::UiContext>())
{
    layout_.Add(std::make_unique<UI::Controls>(store_));
    layout_.Add(std::make_unique<UI::Timeseries>(store_));
    layout_.Add(std::make_unique<UI::Scatter>(store_));
    layout_.Add(std::make_unique<UI::Histogram>(store_));
    layout_.Add(std::make_unique<UI::Viewport3d>(store_));

    receiver_ = std::make_unique<Net::UdpReceiver>(9000, [this](std::span<const uint8_t> raw) {
        if (auto pkt = Net::ParsePacket(raw)) store_->Ingest(*pkt);
    });
}

void App::Run() {
    auto* w = ui_->Window();
    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();
        ui_->BeginFrame();
        layout_.Draw();
        ui_->EndFrame();
        glfwSwapBuffers(w);
    }
}

} // namespace KiloScope::App

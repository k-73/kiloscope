#include "App.hpp"
#include "Net/Packet.hpp"
#include "Ui/PanelControls.hpp"
#include "Ui/PanelTimeseries.hpp"
#include "Ui/PanelScatter.hpp"
#include "Ui/PanelHistogram.hpp"
#include "Ui/PanelViewport3d.hpp"
#include <GLFW/glfw3.h>

namespace ks::app {

App::App()
    : store_(std::make_shared<data::DataStore>())
    , ui_(std::make_unique<ui::UiContext>())
{
    layout_.Add(std::make_unique<ui::PanelControls>(store_));
    layout_.Add(std::make_unique<ui::PanelTimeseries>(store_));
    layout_.Add(std::make_unique<ui::PanelScatter>(store_));
    layout_.Add(std::make_unique<ui::PanelHistogram>(store_));
    layout_.Add(std::make_unique<ui::PanelViewport3d>(store_));

    receiver_ = std::make_unique<net::UdpReceiver>(9000, [this](std::span<const uint8_t> raw) {
        if (auto pkt = net::ParsePacket(raw)) store_->Ingest(*pkt);
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

} // namespace ks::app

#include "UdpReceiver.hpp"
#include "Core/Log.hpp"

namespace KiloScope::Net {

UdpReceiver::UdpReceiver(uint16_t port, Callback cb)
    : callback_(std::move(cb))
    , socket_(ioCtx_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port))
{
    socket_.set_option(asio::socket_base::receive_buffer_size(1024 * 1024));
    StartReceive();
    thread_ = std::jthread([this](std::stop_token st) {
        auto work = asio::make_work_guard(ioCtx_);
        while (!st.stop_requested()) ioCtx_.run_for(std::chrono::milliseconds(10));
    });
    Log::Net().info("Listening on UDP port {}", port);
}

UdpReceiver::~UdpReceiver() {
    thread_.request_stop();
    ioCtx_.stop();
}

void UdpReceiver::StartReceive() {
    socket_.async_receive_from(asio::buffer(buf_), remoteEp_,
        [this](const asio::error_code& ec, size_t bytes) {
            if (!ec && bytes > 0) callback_({buf_.data(), bytes});
            StartReceive();
        });
}

} // namespace KiloScope::Net

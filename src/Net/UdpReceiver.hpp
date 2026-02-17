#pragma once
#include <asio.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <thread>

namespace KiloScope::Net {

class UdpReceiver {
public:
    using Callback = std::function<void(std::span<const uint8_t>)>;

    UdpReceiver(uint16_t port, Callback cb);
    ~UdpReceiver();

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

private:
    void StartReceive();

    Callback callback_;
    asio::io_context ioCtx_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remoteEp_;
    std::array<uint8_t, 65536> buf_{};
    std::jthread thread_;
};

} // namespace KiloScope::Net

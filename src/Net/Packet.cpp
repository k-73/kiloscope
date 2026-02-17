#include "Packet.hpp"
#include <cstring>

namespace ks::net {

std::optional<Packet> ParsePacket(std::span<const uint8_t> raw) {
    if (raw.size() < HeaderSize) return std::nullopt;

    uint32_t magic;
    std::memcpy(&magic, raw.data(), 4);
    if (magic != KscpMagic) return std::nullopt;

    Packet p;
    std::memcpy(&p.channelId,   raw.data() + 4, 2);
    std::memcpy(&p.sampleCount, raw.data() + 6, 2);
    std::memcpy(&p.timestamp,   raw.data() + 8, 8);

    if (raw.size() < HeaderSize + p.sampleCount * SampleSize) return std::nullopt;

    p.samples.resize(p.sampleCount);
    for (uint16_t i = 0; i < p.sampleCount; ++i) {
        auto off = HeaderSize + i * SampleSize;
        std::memcpy(&p.samples[i].timestamp, raw.data() + off,     8);
        std::memcpy(&p.samples[i].value,     raw.data() + off + 8, 8);
    }
    return p;
}

} // namespace ks::net

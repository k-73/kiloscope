#pragma once
#include "Data/Channel.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ks::net {

inline constexpr uint32_t KscpMagic   = 0x4B534350;
inline constexpr size_t   HeaderSize  = 16;
inline constexpr size_t   SampleSize  = 16;

struct Packet {
    uint16_t channelId;
    uint16_t sampleCount;
    double   timestamp;
    std::vector<data::Sample> samples;
};

std::optional<Packet> ParsePacket(std::span<const uint8_t> raw);

} // namespace ks::net

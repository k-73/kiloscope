#pragma once
#include "Data/Sample.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace KiloScope::Net {

inline constexpr uint32_t KscpMagic  = 0x4B534350;
inline constexpr size_t   HeaderSize = 16;
inline constexpr size_t   SampleSize = 16;

struct Packet {
    uint16_t channelId;
    uint16_t sampleCount;
    double   timestamp;
    std::vector<Data::Sample> samples;
};

std::optional<Packet> ParsePacket(std::span<const uint8_t> raw);

} // namespace KiloScope::Net

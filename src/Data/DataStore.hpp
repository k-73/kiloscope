#pragma once
#include "Channel.hpp"
#include "Net/Packet.hpp"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace ks::data {

class DataStore {
public:
    void Ingest(const net::Packet& pkt);

    Channel* GetChannel(uint16_t id);
    const Channel* GetChannel(uint16_t id) const;
    std::vector<uint16_t> ChannelIds() const;

    std::shared_mutex& Mutex() const { return mutex_; }
    void Clear();

    uint64_t TotalPackets() const { return totalPackets_.load(std::memory_order_relaxed); }
    uint64_t TotalSamples() const { return totalSamples_.load(std::memory_order_relaxed); }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint16_t, std::unique_ptr<Channel>> channels_;
    std::atomic<uint64_t> totalPackets_{0}, totalSamples_{0};
};

} // namespace ks::data

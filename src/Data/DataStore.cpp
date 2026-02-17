#include "DataStore.hpp"
#include <algorithm>
#include <mutex>

namespace KiloScope::Data {

void DataStore::Ingest(const Net::Packet& pkt) {
    {
        std::shared_lock lk(mutex_);
        if (auto it = channels_.find(pkt.channelId); it != channels_.end()) {
            for (auto& s : pkt.samples) it->second->Push(s);
            totalPackets_.fetch_add(1, std::memory_order_relaxed);
            totalSamples_.fetch_add(pkt.samples.size(), std::memory_order_relaxed);
            return;
        }
    }
    std::unique_lock lk(mutex_);
    auto& ch = channels_.emplace(pkt.channelId, std::make_unique<Channel>(pkt.channelId)).first->second;
    for (auto& s : pkt.samples) ch->Push(s);
    totalPackets_.fetch_add(1, std::memory_order_relaxed);
    totalSamples_.fetch_add(pkt.samples.size(), std::memory_order_relaxed);
}

Channel* DataStore::GetChannel(uint16_t id) {
    auto it = channels_.find(id);
    return it != channels_.end() ? it->second.get() : nullptr;
}

const Channel* DataStore::GetChannel(uint16_t id) const {
    auto it = channels_.find(id);
    return it != channels_.end() ? it->second.get() : nullptr;
}

std::vector<uint16_t> DataStore::ChannelIds() const {
    std::shared_lock lk(mutex_);
    std::vector<uint16_t> ids;
    ids.reserve(channels_.size());
    for (auto& [id, _] : channels_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void DataStore::Clear() {
    std::unique_lock lk(mutex_);
    channels_.clear();
    totalPackets_.store(0, std::memory_order_relaxed);
    totalSamples_.store(0, std::memory_order_relaxed);
}

} // namespace KiloScope::Data

#pragma once
#include "RingBuffer.hpp"
#include <algorithm>
#include <limits>
#include <string>

namespace KiloScope::Data {

struct Sample { double timestamp, value; };

class Channel {
public:
    explicit Channel(uint16_t id, std::string name = "")
        : id_(id), name_(name.empty() ? "ch" + std::to_string(id) : std::move(name)) {}

    uint16_t Id() const { return id_; }
    const std::string& Name() const { return name_; }
    void SetName(std::string n) { name_ = std::move(n); }

    void Push(const Sample& s) {
        buf_.Push(s);
        minVal_ = std::min(minVal_, s.value);
        maxVal_ = std::max(maxVal_, s.value);
    }

    size_t ReadLast(Sample* out, size_t n) const { return buf_.ReadLast(out, n); }
    size_t WritePos() const { return buf_.WritePos(); }
    size_t ReadAt(Sample* out, size_t n, size_t endPos) const { return buf_.ReadAt(out, n, endPos); }
    size_t Size() const { return buf_.Size(); }

    void Clear() {
        buf_.Clear();
        minVal_ = std::numeric_limits<double>::max();
        maxVal_ = std::numeric_limits<double>::lowest();
    }

    double MinValue() const { return minVal_; }
    double MaxValue() const { return maxVal_; }

private:
    uint16_t id_;
    std::string name_;
    RingBuffer<Sample> buf_;
    double minVal_ = std::numeric_limits<double>::max();
    double maxVal_ = std::numeric_limits<double>::lowest();
};

/// Read up to `maxPts` most-recent samples from multiple channels, aligned
/// to the same logical time point. Snapshots all write positions first
/// (one atomic load each), then reads each channel up to the common minimum.
/// Array sizes must match at compile time.
template <size_t N>
size_t ReadAligned(Channel* (&chs)[N], Sample* (&bufs)[N], size_t maxPts) {
    size_t endPos = SIZE_MAX;
    for (size_t i = 0; i < N; ++i)
        if (chs[i]) endPos = std::min(endPos, chs[i]->WritePos());
    if (endPos == SIZE_MAX) return 0;
    size_t n = 0;
    for (size_t i = 0; i < N; ++i)
        if (chs[i]) n = chs[i]->ReadAt(bufs[i], maxPts, endPos);
    return n;
}

} // namespace KiloScope::Data

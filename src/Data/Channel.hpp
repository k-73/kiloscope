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

} // namespace KiloScope::Data

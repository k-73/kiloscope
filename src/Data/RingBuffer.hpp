#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace KiloScope::Data {

template <typename T, size_t Cap = 131072>
class RingBuffer {
    static_assert((Cap & (Cap - 1)) == 0);
    static_assert(std::is_trivially_copyable_v<T>);

public:
    bool Push(const T& item) noexcept {
        auto w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= Cap) return false;
        buf_[w & Mask] = item;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> Pop() noexcept {
        auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return std::nullopt;
        T item = buf_[r & Mask];
        read_.store(r + 1, std::memory_order_release);
        return item;
    }

    size_t ReadLast(T* out, size_t count) const noexcept {
        auto w = write_.load(std::memory_order_acquire);
        auto avail = w - read_.load(std::memory_order_relaxed);
        if (!avail) return 0;
        auto n = std::min(count, avail);
        auto start = w - n;
        for (size_t i = 0; i < n; ++i) out[i] = buf_[(start + i) & Mask];
        return n;
    }

    size_t Size() const noexcept {
        return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
    }
    bool Empty() const noexcept { return Size() == 0; }
    void Clear() noexcept { read_.store(write_.load(std::memory_order_relaxed), std::memory_order_relaxed); }
    static constexpr size_t Capacity() noexcept { return Cap; }

private:
    static constexpr size_t Mask = Cap - 1;
    alignas(64) std::atomic<size_t> write_{0};
    alignas(64) std::atomic<size_t> read_{0};
    std::array<T, Cap> buf_{};
};

} // namespace KiloScope::Data

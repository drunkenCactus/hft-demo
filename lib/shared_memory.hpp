#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

namespace hft {

struct SharedData {
    std::pair<double, double> best_bid = {0.0, 0.0};
    std::pair<double, double> best_ask = {0.0, 0.0};
    uint64_t ts = 0;
};

const char* const SHARED_MEMORY_NAME = "order_book";
const uint32_t SHARED_MEMORY_SIZE = 64;
const uint32_t CACHE_LINE_SIZE = 64;

template <typename Data, uint32_t BufferSize, uint32_t ConsumersCount>
class RingBuffer {
public:
    RingBuffer() {
        head_ = 0;
        for (uint32_t i = 0; i < ConsumersCount; ++i) {
            tails_[i] = 0;
            active_consumers_[i] = true;
        }
    }

    RingBuffer(const RingBuffer& other) = delete;
    RingBuffer(RingBuffer&& other) = delete;
    RingBuffer& operator=(const RingBuffer& other) = delete;
    RingBuffer& operator=(RingBuffer&& other) = delete;
    ~RingBuffer() = default;

    void Write(const Data& data) noexcept {
        const uint32_t current_head = head_.load(std::memory_order_relaxed);
        const uint32_t new_head = Increment(current_head);
        DisableLaggingConsumers(new_head);
        data_[new_head] = data;
        head_.store(new_head, std::memory_order_release);
    }

    bool Read(Data& data, const uint32_t consumer) noexcept {
        if (!active_consumers_[consumer].load(std::memory_order_acquire)) {
            // consumer is disabled
            return false;
        }
        const uint32_t current_tail = tails_[consumer].load(std::memory_order_relaxed);
        const uint32_t head = head_.load(std::memory_order_acquire);
        if (current_tail == head) {
            // nothing to read
            return false;
        }
        const uint32_t new_tail = Increment(current_tail);
        data = data_[new_tail];
        tails_[consumer].store(new_tail, std::memory_order_release);
        return true;
    }

private:
    uint32_t Increment(const uint32_t current) const noexcept {
        return (current + 1) % BufferSize;
    }

    void DisableLaggingConsumers(const uint32_t new_head) noexcept {
        for (uint32_t i = 0; i < ConsumersCount; ++i) {
            const uint32_t tail = tails_[i].load(std::memory_order_acquire);
            if (new_head == tail) {
                // disable lagging consumer
                active_consumers_[i].store(false, std::memory_order_release);
            }
        }
    }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<uint32_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<uint32_t> tails_[ConsumersCount];
    alignas(CACHE_LINE_SIZE) std::atomic<bool> active_consumers_[ConsumersCount];
    alignas(CACHE_LINE_SIZE) Data data_[BufferSize];

    static_assert(alignof(Data) == CACHE_LINE_SIZE);
    static_assert(sizeof(Data) == CACHE_LINE_SIZE);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
};

}  // namespace hft

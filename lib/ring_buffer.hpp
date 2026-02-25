#pragma once

#include <atomic>
#include <cstdint>

namespace hft {

template <
    typename Data,
    uint32_t Alignment,
    uint32_t BufferLength,
    uint32_t ConsumersCount
> class alignas(Alignment) RingBuffer {
public:
    RingBuffer() {
        head_.value = 0;
        for (uint32_t i = 0; i < ConsumersCount; ++i) {
            tails_[i].value = 0;
            active_consumers_[i].value = true;
        }
    }

    RingBuffer(const RingBuffer& other) = delete;
    RingBuffer(RingBuffer&& other) = delete;
    RingBuffer& operator=(const RingBuffer& other) = delete;
    RingBuffer& operator=(RingBuffer&& other) = delete;
    ~RingBuffer() = default;

    void Write(const Data& data) noexcept {
        const uint32_t current_head = head_.value.load(std::memory_order_relaxed);
        const uint32_t new_head = Increment(current_head);
        DisableLaggingConsumers(new_head);
        data_[new_head] = data;
        head_.value.store(new_head, std::memory_order_release);
    }

    bool Read(Data& data, const uint32_t consumer) noexcept {
        if (!active_consumers_[consumer].value.load(std::memory_order_acquire)) {
            // consumer is disabled
            return false;
        }
        const uint32_t current_tail = tails_[consumer].value.load(std::memory_order_relaxed);
        const uint32_t head = head_.value.load(std::memory_order_acquire);
        if (current_tail == head) {
            // nothing to read
            return false;
        }
        const uint32_t new_tail = Increment(current_tail);
        data = data_[new_tail];
        tails_[consumer].value.store(new_tail, std::memory_order_release);
        return true;
    }

private:
    uint32_t Increment(const uint32_t current) const noexcept {
        // optimization of (current + 1) % BufferLength
        return (current + 1) & (BufferLength - 1);
    }

    void DisableLaggingConsumers(const uint32_t new_head) noexcept {
        for (uint32_t i = 0; i < ConsumersCount; ++i) {
            const uint32_t tail = tails_[i].value.load(std::memory_order_acquire);
            if (new_head == tail) {
                // disable lagging consumer
                active_consumers_[i].value.store(false, std::memory_order_release);
            }
        }
    }

private:
    template <typename T>
    struct alignas(Alignment) AlignedAtomic {
        std::atomic<T> value;
        static_assert(std::atomic<T>::is_always_lock_free);
    };

    AlignedAtomic<uint32_t> head_;
    AlignedAtomic<uint32_t> tails_[ConsumersCount];
    AlignedAtomic<bool> active_consumers_[ConsumersCount];
    Data data_[BufferLength];

    static_assert(
        (BufferLength != 0) && ((BufferLength & (BufferLength - 1)) == 0),
        "BufferLength must be power of 2"
    );
    static_assert(
        alignof(Data) == Alignment,
        "Data must be aligned with Alignment"
    );
};

}  // namespace hft

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
    RingBuffer() noexcept {
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

    [[nodiscard]] bool Read(Data& data, const uint32_t consumer) noexcept {
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

    void ResetConsumer(const uint32_t consumer) noexcept {
        const uint32_t head = head_.value.load(std::memory_order_acquire);
        tails_[consumer].value.store(head, std::memory_order_release);
        active_consumers_[consumer].value.store(true, std::memory_order_release);
    }

private:
    static uint32_t Increment(const uint32_t current) noexcept {
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

private:
    AlignedAtomic<uint32_t> head_;
    AlignedAtomic<uint32_t> tails_[ConsumersCount];
    AlignedAtomic<bool> active_consumers_[ConsumersCount];
    Data data_[BufferLength];

    static_assert(
        (BufferLength != 0) && ((BufferLength & (BufferLength - 1)) == 0),
        "BufferLength must be power of 2"
    );
    static_assert(
        ConsumersCount > 0,
        "ConsumersCount must be greater than zero"
    );
    static_assert(
        Alignment > 0,
        "Alignment must be greater than zero"
    );
    static_assert(
        alignof(Data) == Alignment,
        "Data must be aligned with Alignment"
    );
    static_assert(
        std::is_nothrow_default_constructible_v<Data>,
        "Data must be nothrow default constructible"
    );
    static_assert(
        std::is_copy_assignable_v<Data>,
        "Data must be copy assignable"
    );
    static_assert(
        std::is_trivially_destructible_v<Data>,
        "Data must be trivially destructible"
    );
};

}  // namespace hft

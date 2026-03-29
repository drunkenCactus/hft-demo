#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace hft {

enum class ReadResult : uint8_t {
    kSuccess,
    kBufferIsEmpty,
    kConsumerIsDisabled,
};

template <uint32_t Alignment, typename T>
struct alignas(Alignment) AlignedAtomic {
    std::atomic<T> value;

    AlignedAtomic() : value(T{}) {}

    static_assert(std::atomic<T>::is_always_lock_free);
    static_assert(
        std::is_trivially_default_constructible_v<T>,
        "T must be trivially default constructible"
    );
};

template <uint32_t BufferLength>
constexpr uint32_t Increment(const uint32_t current) noexcept {
    static_assert(
        (BufferLength != 0) && ((BufferLength & (BufferLength - 1)) == 0),
        "BufferLength must be power of 2"
    );
    // optimization of (current + 1) % BufferLength
    return (current + 1) & (BufferLength - 1);
}

template <typename Data, uint32_t Alignment, uint32_t BufferLength>
class alignas(Alignment) SpscRingBuffer {
public:
    SpscRingBuffer() noexcept {
        is_active_consumer_.value.store(true, std::memory_order_release);
    }

    SpscRingBuffer(const SpscRingBuffer& other) = delete;
    SpscRingBuffer(SpscRingBuffer&& other) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer& other) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&& other) = delete;
    ~SpscRingBuffer() = default;

    void Write(const Data& data) noexcept {
        const uint32_t current_head = head_.value.load(std::memory_order_relaxed);
        const uint32_t new_head = Increment<BufferLength>(current_head);
        if (new_head == tail_.value.load(std::memory_order_acquire)) {
            // disable lagging consumer
            is_active_consumer_.value.store(false, std::memory_order_release);
        }
        data_[new_head] = data;
        head_.value.store(new_head, std::memory_order_release);
    }

    [[nodiscard]] ReadResult Read(Data& data) noexcept {
        if (!is_active_consumer_.value.load(std::memory_order_acquire)) {
            return ReadResult::kConsumerIsDisabled;
        }
        const uint32_t current_tail = tail_.value.load(std::memory_order_relaxed);
        const uint32_t head = head_.value.load(std::memory_order_acquire);
        if (current_tail == head) {
            return ReadResult::kBufferIsEmpty;
        }
        const uint32_t new_tail = Increment<BufferLength>(current_tail);
        data = data_[new_tail];
        tail_.value.store(new_tail, std::memory_order_release);
        return ReadResult::kSuccess;
    }

    void ResetConsumer() noexcept {
        const uint32_t head = head_.value.load(std::memory_order_acquire);
        tail_.value.store(head, std::memory_order_release);
        is_active_consumer_.value.store(true, std::memory_order_release);
    }

private:
    AlignedAtomic<Alignment, uint32_t> head_;
    AlignedAtomic<Alignment, uint32_t> tail_;
    AlignedAtomic<Alignment, bool> is_active_consumer_;
    Data data_[BufferLength];

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

#pragma once

#include <atomic>
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

    AlignedAtomic() : value(T{}) {};

    static_assert(std::atomic<T>::is_always_lock_free);
    static_assert(
        std::is_trivially_default_constructible_v<T>,
        "T must be trivially default constructible"
    );
};

template <uint32_t BufferLength>
inline uint32_t Increment(const uint32_t current) noexcept {
    static_assert(
        (BufferLength != 0) && ((BufferLength & (BufferLength - 1)) == 0),
        "BufferLength must be power of 2"
    );
    // optimization of (current + 1) % BufferLength
    return (current + 1) & (BufferLength - 1);
}

}  // namespace hft

#pragma once

#include <cstdint>
#include <stdexcept>

namespace hft {

enum class MemoryRole {
    CREATE_ONLY,
    OPEN_ONLY
};

// MemoryRole::CREATE_ONLY:
//   creates in pre-allocated memory a sequence of Objects using Alignment
//   and returns tuple of pointers to Objects` instances by GetObjects().
// MemoryRole::OPEN_ONLY:
//   fetches in pre-allocated memory a sequence of Objects using Alignment
//   and returns tuple of pointers to Objects` instances by GetObjects().
// Memory should be allocated before constructing ObjectsHandler with size >= ObjectsHandler<>::DataSize().

template <uint32_t Alignment, typename... Objects>
class ObjectsHandler {
public:
    ObjectsHandler(const MemoryRole role, void* const head)
        : role_(role)
        , head_(reinterpret_cast<uintptr_t>(head))
        , objects_(std::make_tuple<Objects* const...>(GetOrCreateObject<Objects>()...))
    {
        if (head_ % Alignment != 0) {
            throw std::runtime_error("ObjectsHandler: head must be aligned with Alignment");
        }
    }

    const std::tuple<Objects* const...>& GetObjects() const noexcept {
        return objects_;
    }

    static constexpr uint32_t DataSize() noexcept {
        return (GetSizeWithPadding<Objects>() + ...);
    }

private:
    template <typename Object>
    [[nodiscard]] Object* const GetOrCreateObject() {
        void* const ptr = reinterpret_cast<void* const>(head_ + offset_);
        offset_ += GetSizeWithPadding<Object>();

        if (role_ == MemoryRole::CREATE_ONLY) {
            return new (ptr) Object;
        } else {
            return static_cast<Object* const>(ptr);
        }
    }

    template <typename Object>
    static constexpr uint32_t GetSizeWithPadding() noexcept {
        const uint32_t remainder = sizeof(Object) % Alignment;
        return remainder == 0
            ? sizeof(Object)
            : sizeof(Object) + Alignment - remainder;
    }

    static_assert(
        (std::is_default_constructible_v<Objects> && ...),
        "Each object must have default constructor"
    );

private:
    const MemoryRole role_;
    const uintptr_t head_;
    uint32_t offset_ = 0;
    std::tuple<Objects* const...> objects_;
};

}  // namespace hft

#pragma once

#include <cstdint>
#include <stdexcept>

namespace hft {

enum class MemoryRole {
    CREATE_ONLY,
    OPEN_ONLY
};

// MemoryRole::CREATE_ONLY:
//   Creates in pre-allocated memory a sequence of Objects using Alignment
//   and returns tuple of pointers to Objects` instances by GetObjects()
// MemoryRole::OPEN_ONLY:
//   Fetches in pre-allocated memory a sequence of Objects using Alignment
//   and returns tuple of pointers to Objects` instances by GetObjects()
// Memory should be allocated before constructing ObjectsHandler with size >= ObjectsHandler<>::DataSize()

template <MemoryRole Role, uint32_t Alignment, typename... Objects>
class ObjectsHandler {
public:
    ObjectsHandler(void* const head)
        : head_(reinterpret_cast<uintptr_t>(head))
    {
        if (head_ % Alignment != 0) {
            throw std::runtime_error("ObjectsHandler: head must be aligned with Alignment");
        }
        objects_ = std::make_tuple<Objects*...>(GetOrCreateObject<Objects>()...);
    }

    std::tuple<Objects*...>& GetObjects() {
        return objects_;
    }

    static constexpr uint32_t DataSize() {
        return (GetSizeWithPadding(sizeof(Objects)) + ...);
    }

private:
    template <typename Object>
    [[nodiscard]] Object* GetOrCreateObject() {
        const uint32_t size = GetSizeWithPadding(sizeof(Object));
        void* const ptr = reinterpret_cast<void* const>(head_ + offset_);
        offset_ += size;

        if constexpr (Role == MemoryRole::CREATE_ONLY) {
            return new (ptr) Object;
        } else {
            return static_cast<Object*>(ptr);
        }
    }

    static uint32_t GetSizeWithPadding(const uint32_t size) {
        const uint32_t remainder = size % Alignment;
        return remainder == 0
            ? size
            : size - remainder + Alignment;
    }

private:
    const uintptr_t head_;
    uint32_t offset_ = 0;
    std::tuple<Objects*...> objects_;
};

}  // namespace hft

#pragma once

#include <lib/common.hpp>
#include <lib/interprocess/interprocess_meta.hpp>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <atomic>
#include <utility>

namespace hft {

namespace bip = boost::interprocess;

template <uint32_t Alignment, typename Object>
constexpr uint32_t GetSizeWithPadding() noexcept {
    const uint32_t remainder = sizeof(Object) % Alignment;
    return remainder == 0
        ? sizeof(Object)
        : sizeof(Object) + Alignment - remainder;
}

template <uint32_t Alignment, typename... Objects>
constexpr uint32_t GetObjectsSize() noexcept {
    return (GetSizeWithPadding<Alignment, Objects>() + ...);
}

template <uint32_t Alignment, typename... Objects>
std::tuple<Objects*...> CreateObjectsInBuffer(void* buffer) noexcept {
    static_assert(
        (std::is_nothrow_default_constructible_v<Objects> && ...),
        "All objects must be nothrow default constructible"
    );
    char* head = static_cast<char*>(buffer);
    uint32_t offset = 0;
    return std::make_tuple((new (head + std::exchange(offset, offset + GetSizeWithPadding<Alignment, Objects>())) Objects)...);
}

template <uint32_t Alignment, typename... Objects>
std::tuple<Objects*...> FetchObjectsFromBuffer(void* buffer) noexcept {
    char* head = static_cast<char*>(buffer);
    uint32_t offset = 0;
    return std::make_tuple((reinterpret_cast<Objects*>(head + std::exchange(offset, offset + GetSizeWithPadding<Alignment, Objects>())))...);
}

enum class MemoryRole {
    CREATE_ONLY,
    OPEN_ONLY
};

template <uint32_t Alignment, typename... Objects>
class SharedMemory {
private:
    struct alignas(Alignment) Meta {
        const uint32_t magic = IPC_MAGIC;
        const uint32_t version = IPC_VERSION;
        std::atomic<uint32_t> heartbeat_seconds{0};
        static_assert(std::atomic<uint32_t>::is_always_lock_free);
    };

public:
    SharedMemory(const char* const name, const MemoryRole role)
        : role_(role)
    {
        if (role_ == MemoryRole::CREATE_ONLY) {
            memory_ = bip::shared_memory_object(bip::create_only, name, bip::read_write);
            memory_.truncate(GetObjectsSize<Alignment, Meta, Objects...>());
        } else {
            memory_ = bip::shared_memory_object(bip::open_only, name, bip::read_write);
        }

        region_ = bip::mapped_region(memory_, bip::read_write);
        if (reinterpret_cast<uintptr_t>(region_.get_address()) % Alignment != 0) {
            throw std::runtime_error("invalid region alignment");
        }
        if (region_.get_size() != GetObjectsSize<Alignment, Meta, Objects...>()) {
            throw std::runtime_error("invalid region size");
        }
        // prevent flushing on disk
        if (mlock(region_.get_address(), region_.get_size()) != 0) {
            int error_code = errno;
            std::string reason;
            switch (error_code) {
                case ENOMEM:
                    reason = "not enough memory or RLIMIT_MEMLOCK exceeded";
                    break;
                case EINVAL:
                    reason = "address is not page-aligned or length is 0";
                    break;
                case EPERM:
                    reason = "permission denied (check limits or capabilities)";
                    break;
                default:
                    reason = "unknown error (" + std::to_string(error_code) + ")";
            }
            throw std::runtime_error("mlock failed: " + reason);
        }

        auto buffer_data = role_ == MemoryRole::CREATE_ONLY
            ? CreateObjectsInBuffer<Alignment, Meta, Objects...>(region_.get_address())
            : FetchObjectsFromBuffer<Alignment, Meta, Objects...>(region_.get_address());
        std::apply(
            [this](Meta* meta, Objects*... objects) {
                meta_ = meta;
                objects_ = std::make_tuple(objects...);
            },
            std::move(buffer_data)
        );

        if (role_ == MemoryRole::OPEN_ONLY) {
            if (meta_->magic != IPC_MAGIC) {
                throw std::runtime_error("shared memory magic is missed");
            }
            if (meta_->version != IPC_VERSION) {
                throw std::runtime_error("shared memory version conflict");
            }
        }
    }

    SharedMemory(const SharedMemory& other) = delete;
    SharedMemory(SharedMemory&& other) = delete;
    SharedMemory& operator=(const SharedMemory& other) = delete;
    SharedMemory& operator=(SharedMemory&& other) = delete;
    ~SharedMemory() = default;

    const std::tuple<Objects*...>& GetObjects() const noexcept {
        return objects_;
    }

    bool IsProducerAlive(const uint32_t treshold_seconds) const noexcept {
        return meta_->heartbeat_seconds.load(std::memory_order_acquire) + treshold_seconds > NowSeconds();
    }

    void UpdateHeartbeat() noexcept {
        meta_->heartbeat_seconds.store(NowSeconds(), std::memory_order_release);
    }

private:
    const MemoryRole role_;
    bip::shared_memory_object memory_;
    bip::mapped_region region_;
    std::tuple<Objects*...> objects_ = std::make_tuple(static_cast<Objects*>(nullptr)...);
    Meta* meta_ = nullptr;

    static_assert(
        Alignment > 0,
        "Alignment must be greater than zero"
    );
    static_assert(
        ((alignof(Objects) == Alignment) && ...),
        "All objects must be aligned with Alignment"
    );
    static_assert(
        (std::is_nothrow_default_constructible_v<Objects> && ...),
        "All objects must be nothrow default constructible"
    );
    static_assert(
        (std::is_trivially_destructible_v<Objects> && ...),
        "All objects must be trivially destructible"
    );
};

inline void RemoveSharedMemory(const char* const name) {
    bip::shared_memory_object::remove(name);
}

}  // namespace hft

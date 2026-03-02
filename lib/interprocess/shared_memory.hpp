#pragma once

#include <lib/common.hpp>
#include <lib/interprocess/interprocess_meta.hpp>
#include <lib/interprocess/objects_handler.hpp>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <exception>

namespace hft {

class SharedMemoryException : public std::exception {};
class FailedToCreateSharedMemory : public SharedMemoryException {};
class FailedToOpenSharedMemory : public SharedMemoryException {};
class FailedToMapRegion : public SharedMemoryException {};
class FailedToHandleObjects : public SharedMemoryException {};
class SharedMemoryVersionConflict : public SharedMemoryException {};
class SharedMemoryIsEmpty : public SharedMemoryException {};

template <uint32_t Alignment, typename... Objects>
class SharedMemory {
private:
    struct alignas(Alignment) Meta {
        const uint32_t magic = IPC_MAGIC;
        const uint32_t version = IPC_VERSION;
        uint32_t heartbeart_seconds = 0;
    };

    using ShmObjectsHandler = ObjectsHandler<Alignment, Meta, Objects...>;

public:
    SharedMemory(const char* const name, const MemoryRole role)
        : name_(name)
        , role_(role)
    {
        if (role_ == MemoryRole::CREATE_ONLY) {
            CreateSharedMemory();
            MapRegion();
            HandleObjects();
        } else {
            OpenSharedMemory();
            MapRegion();
            HandleObjects();
            CheckMeta();
        }
    }

    const std::tuple<Objects*...>& GetObjects() const noexcept {
        return objects_;
    }

    bool IsProducerAlive(const uint32_t treshold_seconds) const noexcept {
        return meta_->heartbeart_seconds + treshold_seconds > NowSeconds();
    }

    void UpdateHeartbeart() const noexcept {
        meta_->heartbeart_seconds = NowSeconds();
    }

private:
    void CreateSharedMemory() {
        try {
            boost::interprocess::shared_memory_object::remove(name_);
            memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::create_only,
                name_,
                ipc_mode_
            );
            memory_.truncate(ShmObjectsHandler::DataSize());
        } catch (const std::exception& e) {
            throw FailedToCreateSharedMemory();
        }
    }

    void OpenSharedMemory() {
        try {
            memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::open_only,
                name_,
                ipc_mode_
            );
        } catch (const std::exception& e) {
            throw FailedToOpenSharedMemory();
        }
    }

    void MapRegion() {
        try {
            region_ = boost::interprocess::mapped_region(memory_, ipc_mode_);
            // prevent flushing on disk
            mlock(region_.get_address(), region_.get_size());
        } catch (const std::exception& e) {
            throw FailedToMapRegion();
        }
    }

    void HandleObjects() {
        try {
            ShmObjectsHandler objects_handler(role_, region_.get_address());
            std::apply(
                [this](Meta* const meta, Objects* const... objects) {
                    meta_ = meta;
                    objects_ = std::make_tuple(objects...);
                },
                objects_handler.GetObjects()
            );
        } catch (const std::exception& e) {
            throw FailedToHandleObjects();
        }
    }

    void CheckMeta() const {
        if (meta_->magic != IPC_MAGIC) {
            throw SharedMemoryIsEmpty();
        }
        if (meta_->version != IPC_VERSION) {
            throw SharedMemoryVersionConflict();
        }
    }

private:
    constexpr static boost::interprocess::mode_t ipc_mode_ = boost::interprocess::read_write;
    const char* const name_;
    const MemoryRole role_;
    boost::interprocess::shared_memory_object memory_;
    boost::interprocess::mapped_region region_;
    std::tuple<Objects*...> objects_;
    Meta* meta_;
};

}  // namespace hft

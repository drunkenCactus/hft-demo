#pragma once

#include <lib/objects_handler.hpp>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

namespace hft {

template <MemoryRole Role, uint32_t Alignment, typename... Objects>
class SharedMemory {
public:
    struct alignas(Alignment) Meta {
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t heartbeart_seconds = 0;
    };

    using ShmObjectsHandler = ObjectsHandler<Role, Alignment, Meta, Objects...>;

    SharedMemory(const char* const name) {
        if constexpr (Role == MemoryRole::CREATE_ONLY) {
            boost::interprocess::shared_memory_object::remove(name);
            memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::create_only,
                name,
                ipc_mode_
            );
            memory_.truncate(ShmObjectsHandler::DataSize());
        } else {
            memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::open_only,
                name,
                ipc_mode_
            );
        }
        region_ = boost::interprocess::mapped_region(memory_, ipc_mode_);
        // prevent flushing on disk
        mlock(region_.get_address(), region_.get_size());

        ShmObjectsHandler objects_handler(region_.get_address());
        std::apply(
            [this](Meta* meta, Objects*... objects) {
                meta_ = meta;
                objects_ = std::make_tuple(objects...);
            },
            objects_handler.GetObjects()
        );
    }

    std::tuple<Objects*...> GetObjects() {
        return objects_;
    }

private:
    constexpr static boost::interprocess::mode_t ipc_mode_ = boost::interprocess::read_write;
    boost::interprocess::shared_memory_object memory_;
    boost::interprocess::mapped_region region_;
    std::tuple<Objects*...> objects_;
    Meta* meta_;
};

}  // namespace hft

#pragma once

#include <chrono>
#include <cstdint>

namespace hft {

inline uint64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}  // namespace hft

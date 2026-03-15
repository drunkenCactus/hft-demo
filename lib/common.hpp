#pragma once

#include <chrono>
#include <cstdint>

namespace hft {

inline uint64_t NowSeconds() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline uint64_t NowNanoseconds() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline uint64_t NowMicroseconds() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}  // namespace hft

#pragma once

#include <lib/log_common.hpp>

#include <cstdint>
#include <utility>

namespace hft {

template <uint32_t Alignment>
struct alignas(Alignment) BestBidAskData {
    std::pair<double, double> best_bid = {0.0, 0.0};
    std::pair<double, double> best_ask = {0.0, 0.0};
};

template <uint32_t Alignment, uint32_t DataSize>
struct alignas(Alignment) ObserverData {
    constexpr static uint32_t message_size = DataSize - sizeof(uint64_t) - sizeof(LogLevel);

    static_assert(
        DataSize > sizeof(uint64_t) + sizeof(LogLevel)
        && DataSize >= Alignment
        && DataSize % Alignment == 0,
        "DataSize is incorrect"
    );

    uint64_t timestamp_ns = 0;
    LogLevel level = LogLevel::INFO;
    char message[message_size];
};

}  // namespace hft

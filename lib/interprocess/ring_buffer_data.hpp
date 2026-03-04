#pragma once

#include <cstdint>
#include <utility>

namespace hft {

template <uint32_t Alignment>
struct alignas(Alignment) BestBidAskData {
    std::pair<double, double> best_bid = {0.0, 0.0};
    std::pair<double, double> best_ask = {0.0, 0.0};
    uint64_t ts = 0;
};

template <uint32_t Alignment>
struct alignas(Alignment) ObserverData {
    uint64_t timestamp_ns = 0;
    constexpr static uint32_t message_size = 56;
    char message[message_size];
};

}  // namespace hft

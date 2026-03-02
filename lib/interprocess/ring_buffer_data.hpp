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

}  // namespace hft

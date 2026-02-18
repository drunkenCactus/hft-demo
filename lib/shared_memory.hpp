#pragma once

#include <cstdint>
#include <utility>

namespace hft {

struct SharedData {
    std::pair<double, double> best_bid = {0.0, 0.0};
    std::pair<double, double> best_ask = {0.0, 0.0};
    uint64_t ts = 0;
};

const char* const SHARED_MEMORY_NAME = "order_book";
const uint32_t SHARED_MEMORY_SIZE = 64;

}  // namespace hft

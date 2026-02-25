#pragma once

#include <lib/ring_buffer.hpp>

#include <cstdint>
#include <utility>

namespace hft {

const char* const SHARED_MEMORY_NAME = "order_book";
constexpr uint32_t SHARED_MEMORY_SIZE = 4096;
constexpr uint32_t CACHE_LINE_SIZE = 64;
constexpr uint32_t RING_BUFFER_LENGTH = 32;
constexpr uint32_t CONSUMERS_COUNT = 1;

struct alignas(CACHE_LINE_SIZE) BestBidAskData {
    std::pair<double, double> best_bid = {0.0, 0.0};
    std::pair<double, double> best_ask = {0.0, 0.0};
    uint64_t ts = 0;
};

using BestBidAskRingBuffer = RingBuffer<
    BestBidAskData,
    CACHE_LINE_SIZE,
    RING_BUFFER_LENGTH,
    CONSUMERS_COUNT
>;

static_assert(
    sizeof(BestBidAskRingBuffer) <= SHARED_MEMORY_SIZE,
    "Ring buffer total size should`t be greater than shared memory size"
);

}  // namespace hft

#pragma once

#include <lib/interprocess/ring_buffer_data.hpp>
#include <lib/interprocess/spmc_ring_buffer.hpp>
#include <lib/interprocess/spsc_ring_buffer.hpp>
#include <lib/interprocess/shared_memory.hpp>

#include <cstdint>

namespace hft {

constexpr uint32_t CACHE_LINE_SIZE = 64;
constexpr uint32_t ORDER_BOOK_DEPTH = 100;

// * * * Feeder -> Trader(s) * * *

const char* const SHM_NAME_MARKET_DATA = "market_data";

using OrderBookUpdate = OrderBookUpdate_<CACHE_LINE_SIZE>;
using Trade = Trade_<CACHE_LINE_SIZE>;

using OrderBookUpdateRingBuffer = SpmcRingBuffer<
    OrderBookUpdate,
    CACHE_LINE_SIZE,
    1024 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using TradeRingBuffer = SpmcRingBuffer<
    Trade,
    CACHE_LINE_SIZE,
    1024 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using OrderBookSnapshot = OrderBookSnapshot_<CACHE_LINE_SIZE, ORDER_BOOK_DEPTH>;

using ShmMarketData = SharedMemory<
    CACHE_LINE_SIZE,
    OrderBookUpdateRingBuffer,
    TradeRingBuffer,
    OrderBookSnapshot
>;

// * * * Any service -> Observer * * *

const char* const SHM_NAME_FEEDER_TO_OBSERVER = "feeder_to_observer";
const char* const SHM_NAME_TRADER_TO_OBSERVER = "trader_to_observer";

using ObserverData = ObserverData_<CACHE_LINE_SIZE, 2 * CACHE_LINE_SIZE>;

using ObserverRingBuffer = SpscRingBuffer<
    ObserverData,
    CACHE_LINE_SIZE,
    1024 /*BufferLength*/
>;

using ShmToObserver = SharedMemory<
    CACHE_LINE_SIZE,
    ObserverRingBuffer
>;

}  // namespace hft

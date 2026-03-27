#pragma once

#include <lib/interprocess/ring_buffer_data.hpp>
#include <lib/interprocess/spmc_ring_buffer.hpp>
#include <lib/interprocess/spsc_ring_buffer.hpp>
#include <lib/interprocess/shared_memory.hpp>
#include <lib/local_order_book.hpp>

#include <cstdint>

namespace hft {

constexpr uint32_t kCacheLineSize = 64;
constexpr uint32_t kOrderBookDepth = 100;
constexpr uint32_t kPriceShift = 8;
constexpr uint32_t kQuantityShift = 8;

// * * * Market Data * * *

const char* const kShmNameMarketData = "market_data";

using OrderBookUpdate = OrderBookUpdate_<kCacheLineSize>;
using Trade = Trade_<kCacheLineSize>;

using OrderBookUpdateRingBuffer = SpmcRingBuffer<
    OrderBookUpdate,
    kCacheLineSize,
    4096 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using TradeRingBuffer = SpmcRingBuffer<
    Trade,
    kCacheLineSize,
    4096 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using OrderBookSnapshot = OrderBookSnapshot_<kCacheLineSize, kOrderBookDepth>;

using ShmMarketData = SharedMemory<
    kCacheLineSize,
    OrderBookUpdateRingBuffer,
    TradeRingBuffer
>;

using OrderBook = OrderBook_<kOrderBookDepth>;

using Order = Order_<kCacheLineSize>;

// * * * Observer * * *

const char* const kShmNameFeederToObserver = "feeder_to_observer";
const char* const kShmNameTraderToObserver = "trader_to_observer";

using ObserverData = ObserverData_<kCacheLineSize, 2 * kCacheLineSize>;

using ObserverRingBuffer = SpscRingBuffer<
    ObserverData,
    kCacheLineSize,
    1024 /*BufferLength*/
>;

using ShmToObserver = SharedMemory<
    kCacheLineSize,
    ObserverRingBuffer
>;

}  // namespace hft

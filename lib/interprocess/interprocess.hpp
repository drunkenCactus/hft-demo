#pragma once

#include <lib/interprocess/ring_buffer_data.hpp>
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

using OrderBookUpdate = OrderBookUpdate_<kCacheLineSize>;
using Trade = Trade_<kCacheLineSize>;

using OrderBookUpdateRingBuffer = SpscRingBuffer<
    OrderBookUpdate,
    kCacheLineSize,
    4096 /*BufferLength*/
>;

using TradeRingBuffer = SpscRingBuffer<
    Trade,
    kCacheLineSize,
    4096 /*BufferLength*/
>;

using OrderBookSnapshot = OrderBookSnapshot_<kCacheLineSize, kOrderBookDepth>;

using ShmMarketData = SharedMemory<
    kCacheLineSize,
    OrderBookUpdateRingBuffer,
    TradeRingBuffer
>;

using OrderBook = OrderBook_<kOrderBookDepth>;

using Order = Order_<kCacheLineSize>;

using OrderRingBuffer = SpscRingBuffer<
    Order,
    kCacheLineSize,
    1024 /*BufferLength*/
>;

using ShmOrder = SharedMemory<
    kCacheLineSize,
    OrderRingBuffer
>;

// * * * Observer * * *

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

using LatencyNsSample = LatencyNsSample_<kCacheLineSize>;

using LatencyRingBuffer = SpscRingBuffer<
    LatencyNsSample,
    kCacheLineSize,
    16384 /*BufferLength*/
>;

using ShmLatency = SharedMemory<
    kCacheLineSize,
    LatencyRingBuffer
>;

}  // namespace hft

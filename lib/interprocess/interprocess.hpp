#pragma once

#include <lib/interprocess/ring_buffer_data.hpp>
#include <lib/interprocess/spmc_ring_buffer.hpp>
#include <lib/interprocess/spsc_ring_buffer.hpp>
#include <lib/interprocess/shared_memory.hpp>

#include <cstdint>

namespace hft {

constexpr uint32_t CACHE_LINE_SIZE = 64;

// * * * MD Feeder -> Trading Engine(s) * * *

const char* const SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE = "md_feeder_to_trading_engine";

using BestBidAskRingBufferData = BestBidAskData<CACHE_LINE_SIZE>;

using BestBidAskRingBuffer = SpmcRingBuffer<
    BestBidAskRingBufferData,
    CACHE_LINE_SIZE,
    1024 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using ShmMdFeederToTradingEngine = SharedMemory<
    CACHE_LINE_SIZE,
    BestBidAskRingBuffer
>;

// * * * Any service -> Observer * * *

const char* const SHM_NAME_MD_FEEDER_TO_OBSERVER = "md_feeder_to_observer";
const char* const SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER = "md_trading_engine_btc_to_observer";

using ObserverRingBufferData = ObserverData<CACHE_LINE_SIZE, 2 * CACHE_LINE_SIZE>;

using ObserverRingBuffer = SpscRingBuffer<
    ObserverRingBufferData,
    CACHE_LINE_SIZE,
    1024 /*BufferLength*/
>;

using ShmToObserver = SharedMemory<
    CACHE_LINE_SIZE,
    ObserverRingBuffer
>;

}  // namespace hft

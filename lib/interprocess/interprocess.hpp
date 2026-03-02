#pragma once

#include <lib/interprocess/ring_buffer.hpp>
#include <lib/interprocess/ring_buffer_data.hpp>
#include <lib/interprocess/shared_memory.hpp>

#include <cstdint>

namespace hft {

constexpr uint32_t CACHE_LINE_SIZE = 64;

// * * * MD Feeder -> Trading Engine(s) * * *

const char* const SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE = "md_feeder_to_trading_engine";

using BestBidAskRingBufferData = BestBidAskData<CACHE_LINE_SIZE>;

using BestBidAskRingBuffer = RingBuffer<
    BestBidAskRingBufferData,
    CACHE_LINE_SIZE,
    32 /*BufferLength*/,
    1 /*ConsumersCount*/
>;

using ShmMdFeederToTradingEngine = SharedMemory<
    CACHE_LINE_SIZE,
    BestBidAskRingBuffer
>;

}  // namespace hft

#pragma once

#include <lib/log_common.hpp>

#include <cstdint>

namespace hft {

enum class Symbol : uint8_t {
    UNKNOWN,
    BTCUSDT,
    ETHUSDT,
};

struct MessageMeta {
    uint64_t event_timestamp_microseconds = 0;
    uint64_t parsing_timestamp_microseconds = 0;
};

template <uint32_t Alignment, uint32_t Depth>
struct alignas(Alignment) OrderBookSnapshot_ {
    MessageMeta meta;
    uint64_t last_update_id = 0;
    double bids_prices[Depth];
    double bids_quantities[Depth];
    double asks_prices[Depth];
    double asks_quantities[Depth];
    uint32_t bids_depth = 0;
    uint32_t asks_depth = 0;
    Symbol symbol = Symbol::UNKNOWN;

    static_assert(
        Depth > 0,
        "Depth must be greater than zero"
    );
};

template <uint32_t Alignment>
struct alignas(Alignment) OrderBookUpdate_ {
    enum class Type : uint8_t {
        BID,
        ASK,
    };

    MessageMeta meta;
    uint64_t first_update_id = 0;
    uint64_t last_update_id = 0;
    double price = 0.0;
    double quantity = 0.0;
    Type type = Type::BID;
    Symbol symbol = Symbol::UNKNOWN;
};

template <uint32_t Alignment>
struct alignas(Alignment) Trade_ {
    MessageMeta meta;
    double price = 0.0;
    double quantity = 0.0;
    Symbol symbol = Symbol::UNKNOWN;
};

template <uint32_t Alignment, uint32_t DataSize>
struct alignas(Alignment) ObserverData_ {
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

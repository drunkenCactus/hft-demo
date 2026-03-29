#pragma once

#include <lib/log_common.hpp>

#include <cstdint>

namespace hft {

enum class Symbol : uint8_t {
    kUnknown,
    kBtcUsdt,
    kEthUsdt,
};

struct MessageMeta {
    uint64_t event_timestamp_microseconds = 0;
    uint64_t parsing_timestamp_microseconds = 0;
};

template <uint32_t Alignment, uint32_t Depth>
struct alignas(Alignment) OrderBookSnapshot_ {
    MessageMeta meta;
    uint64_t last_update_id = 0;
    uint64_t bids_prices[Depth]{};
    uint64_t bids_quantities[Depth]{};
    uint64_t asks_prices[Depth]{};
    uint64_t asks_quantities[Depth]{};
    uint32_t bids_depth = 0;
    uint32_t asks_depth = 0;
    Symbol symbol = Symbol::kUnknown;

    static_assert(
        Depth > 0,
        "Depth must be greater than zero"
    );
};

template <uint32_t Alignment>
struct alignas(Alignment) OrderBookUpdate_ {
    enum class Type : uint8_t {
        kBid,
        kAsk,
    };

    MessageMeta meta;
    uint64_t first_update_id = 0;
    uint64_t last_update_id = 0;
    uint64_t price = 0;
    uint64_t quantity = 0;
    Type type = Type::kBid;
    Symbol symbol = Symbol::kUnknown;
};

template <uint32_t Alignment>
struct alignas(Alignment) Trade_ {
    MessageMeta meta;
    uint64_t price = 0;
    uint64_t quantity = 0;
    Symbol symbol = Symbol::kUnknown;
    bool is_buyer_maker = false;
};

template <uint32_t Alignment>
struct alignas(Alignment) Order_ {
    enum class Type : uint8_t {
        kBuy,
        kSell,
    };

    uint64_t price = 0;
    uint64_t quantity = 0;
    Type type = Type::kBuy;
    Symbol symbol = Symbol::kUnknown;
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
    LogLevel level = LogLevel::kInfo;
    char message[message_size];
};

}  // namespace hft

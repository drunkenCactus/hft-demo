#pragma once

#include <lib/interprocess/interprocess.hpp>

#include <functional>
#include <string_view>

namespace hft {

bool ParseEvent(
    std::string_view json,
    uint64_t steady_nanoseconds,
    std::function<void(const OrderBookUpdate&)> order_book_update_callback,
    std::function<void(const Trade&)> trade_callback
) noexcept;

bool ParseDepthEvent(
    std::string_view json,
    std::function<void(const OrderBookUpdate&)> callback,
    uint64_t steady_nanoseconds = 0
) noexcept;

bool ParseTradeEvent(
    std::string_view json,
    std::function<void(const Trade&)> callback,
    uint64_t steady_nanoseconds = 0
) noexcept;

bool ParseOrderBookSnapshot(std::string_view json, std::function<void(const OrderBookSnapshot&)> callback) noexcept;

}  // namespace hft

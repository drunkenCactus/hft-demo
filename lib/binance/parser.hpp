#pragma once

#include <lib/interprocess/interprocess.hpp>

#include <functional>
#include <string_view>

namespace hft {

bool ParseEvent(
    std::string_view json,
    std::function<void(OrderBookUpdate&)> order_book_update_callback,
    std::function<void(Trade&)> trade_callback
) noexcept;

bool ParseDepthEvent(
    std::string_view json,
    std::function<void(OrderBookUpdate&)> callback
) noexcept;

bool ParseTradeEvent(
    std::string_view json,
    std::function<void(Trade&)> callback
) noexcept;

bool ParseOrderBookSnapshot(std::string_view json, std::function<void(const OrderBookSnapshot&)> callback) noexcept;

}  // namespace hft

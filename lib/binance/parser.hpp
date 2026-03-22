#pragma once

#include <lib/interprocess/interprocess.hpp>

#include <functional>
#include <string_view>

namespace hft {

constexpr uint32_t PRICE_SHIFT = 8;
constexpr uint32_t QUANTITY_SHIFT = 8;

bool ParseEvent(
    std::string_view json,
    std::function<void(const OrderBookUpdate&)> order_book_update_callback,
    std::function<void(const Trade&)> trade_callback
) noexcept;

bool ParseDepthEvent(std::string_view json, std::function<void(const OrderBookUpdate&)> callback) noexcept;

bool ParseTradeEvent(std::string_view json, std::function<void(const Trade&)> callback) noexcept;

bool ParseOrderBookSnapshot(std::string_view json, std::function<void(const OrderBookSnapshot&)> callback) noexcept;

}  // namespace hft

#pragma once

#include <lib/interprocess/interprocess.hpp>

#include <functional>
#include <string_view>

namespace hft {

// Parses Binance combined stream message: {"stream":"<name>","data":<payload>}.
// Dispatches by stream: btcusdt@depth@100ms / btcusdt@depth → ParseDepthEvent(data, order_book_update_callback);
// btcusdt@trade → ParseTradeEvent(data, trade_callback). Returns true on success.
bool ParseEvent(
    std::string_view json,
    std::function<void(const OrderBookUpdate&)> order_book_update_callback,
    std::function<void(const Trade&)> trade_callback
) noexcept;

// Parses Binance depth update event (e.g. from @depth@100ms stream).
// Invokes callback for each bid and each ask from "b"/"a" arrays. Returns true on success.
// Callback must not throw; otherwise std::terminate() is called.
bool ParseDepthEvent(std::string_view json, std::function<void(const OrderBookUpdate&)> callback) noexcept;

// Parses Binance trade event (e.g. from @trade stream).
// Invokes callback with the parsed Trade. Returns true on success.
// Callback must not throw; otherwise std::terminate() is called.
bool ParseTradeEvent(std::string_view json, std::function<void(const Trade&)> callback) noexcept;

// Parses Binance order book snapshot (REST API /depth).
// Invokes callback with the parsed OrderBookSnapshot. Returns true on success.
// Callback must not throw; otherwise std::terminate() is called.
bool ParseOrderBookSnapshot(std::string_view json, std::function<void(const OrderBookSnapshot&)> callback) noexcept;

}  // namespace hft

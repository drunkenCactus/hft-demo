#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hft {

enum class TraderId : std::uint8_t {
    kBtcUsdt = 0,
    kEthUsdt = 1,
};

constexpr std::size_t kTraderCount = 2;

constexpr std::size_t TraderIdToIndex(TraderId id) noexcept {
    return static_cast<std::size_t>(id);
}

constexpr const char* BinanceSymbol(TraderId id) noexcept {
    switch (id) {
        case TraderId::kBtcUsdt:
            return "BTCUSDT";
        case TraderId::kEthUsdt:
            return "ETHUSDT";
    }
    return "";
}

TraderId ParseTraderIdOrAbort(std::string_view role);

}  // namespace hft

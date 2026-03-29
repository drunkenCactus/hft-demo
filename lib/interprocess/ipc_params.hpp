#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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

TraderId ParseTraderIdOrAbort(std::string_view role) noexcept;

struct TraderConfig {
    std::string market_data_shm;
    std::string trader_observer_shm;
};

const char* IpcFeederToObserverShmName();

const TraderConfig& GetTraderConfig(TraderId id);

}  // namespace hft

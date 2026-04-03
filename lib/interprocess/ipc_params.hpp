#pragma once

#include <lib/interprocess/trader_id.hpp>

#include <string>
#include <string_view>

namespace hft {

struct TraderConfig {
    std::string market_data_shm;
    std::string trader_observer_shm;
    std::string order_shm;
};

TraderId ParseTraderIdOrAbort(std::string_view role) noexcept;

const char* IpcFeederToObserverShmName();

const char* IpcExecutorToObserverShmName();

const char* IpcLatencyShmName();

const TraderConfig& GetTraderConfig(TraderId id);

}  // namespace hft

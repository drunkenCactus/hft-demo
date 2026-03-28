#pragma once

#include <lib/interprocess/trader_id.hpp>

#include <string>

namespace hft {

struct TraderConfig {
    std::string market_data_shm;
    std::string trader_observer_shm;
};

const char* IpcFeederToObserverShmName();

const TraderConfig& GetTraderConfig(TraderId id);

}  // namespace hft

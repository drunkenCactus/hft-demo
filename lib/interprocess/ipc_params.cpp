#include <lib/interprocess/ipc_params.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace hft {

namespace {

std::once_flag g_once;
std::string g_feeder_observer_shm;
std::array<TraderConfig, kTraderCount> g_trader_configs;

void RequireEnv(const char* env_name, std::string& out) {
    const char* v = std::getenv(env_name);
    if (v == nullptr || v[0] == '\0') {
        std::fprintf(
            stderr,
            "FATAL: environment variable %s must be set and non-empty\n",
            env_name
        );
        std::abort();
    }
    out = v;
}

std::string EnvNameIndexed(const char* prefix, std::size_t index) {
    return std::string(prefix) + std::to_string(index);
}

void LoadAll() {
    RequireEnv("HFT_IPC_SHM_FEEDER_TO_OBSERVER", g_feeder_observer_shm);
    for (std::size_t i = 0; i < kTraderCount; ++i) {
        RequireEnv(EnvNameIndexed("HFT_IPC_SHM_MARKET_DATA_", i).c_str(), g_trader_configs[i].market_data_shm);
        RequireEnv(EnvNameIndexed("HFT_IPC_SHM_TRADER_OBSERVER_", i).c_str(), g_trader_configs[i].trader_observer_shm);
    }
}

void EnsureIpcParamsLoaded() {
    std::call_once(g_once, LoadAll);
}

}  // namespace

TraderId ParseTraderIdOrAbort(std::string_view role) {
    if (role == "trader_btcusdt") {
        return TraderId::kBtcUsdt;
    }
    if (role == "trader_ethusdt") {
        return TraderId::kEthUsdt;
    }
    std::fprintf(
        stderr,
        "FATAL: unknown trader id (expected trader_btcusdt|trader_ethusdt), got \"%.*s\"\n",
        static_cast<int>(role.size()),
        role.data()
    );
    std::abort();
}

const char* IpcFeederToObserverShmName() {
    EnsureIpcParamsLoaded();
    return g_feeder_observer_shm.c_str();
}

const TraderConfig& GetTraderConfig(TraderId id) {
    EnsureIpcParamsLoaded();
    return g_trader_configs[TraderIdToIndex(id)];
}

}  // namespace hft

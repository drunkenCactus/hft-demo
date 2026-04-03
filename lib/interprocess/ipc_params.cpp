#include <lib/interprocess/ipc_params.hpp>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace hft {

namespace {

std::once_flag g_once;
std::string g_feeder_observer_shm;
std::string g_executor_observer_shm;
std::string g_latency_shm;
ArrayByTraderId<TraderConfig> g_trader_configs;

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
    RequireEnv("HFT_IPC_SHM_EXECUTOR_TO_OBSERVER", g_executor_observer_shm);
    RequireEnv("HFT_IPC_SHM_LATENCY", g_latency_shm);
    for (TraderId id : kTraderIds) {
        const std::size_t idx = std::to_underlying(id);
        RequireEnv(EnvNameIndexed("HFT_IPC_SHM_MARKET_DATA_", idx).c_str(), g_trader_configs[id].market_data_shm);
        RequireEnv(EnvNameIndexed("HFT_IPC_SHM_TRADER_OBSERVER_", idx).c_str(), g_trader_configs[id].trader_observer_shm);
        RequireEnv(EnvNameIndexed("HFT_IPC_SHM_ORDER_", idx).c_str(), g_trader_configs[id].order_shm);
    }
}

void EnsureIpcParamsLoaded() {
    std::call_once(g_once, LoadAll);
}

}  // namespace

TraderId ParseTraderIdOrAbort(std::string_view role) noexcept {
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

const char* IpcExecutorToObserverShmName() {
    EnsureIpcParamsLoaded();
    return g_executor_observer_shm.c_str();
}

const char* IpcLatencyShmName() {
    EnsureIpcParamsLoaded();
    return g_latency_shm.c_str();
}

const TraderConfig& GetTraderConfig(TraderId id) {
    EnsureIpcParamsLoaded();
    return g_trader_configs[id];
}

}  // namespace hft

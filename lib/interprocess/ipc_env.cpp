#include <lib/interprocess/ipc_env.hpp>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace hft {

namespace {

std::once_flag g_ipc_env_once;
std::string g_market_data_shm;
std::string g_feeder_observer_shm;
std::string g_trader_observer_shm;

void AbortMissingEnv(const char* env_name) {
    std::fprintf(
        stderr,
        "FATAL: environment variable %s must be set and non-empty\n",
        env_name
    );
    std::abort();
}

void RequireEnv(const char* env_name, std::string& out) {
    const char* v = std::getenv(env_name);
    if (v == nullptr || v[0] == '\0') {
        AbortMissingEnv(env_name);
    }
    out = v;
}

void LoadFromEnvironmentOnce() {
    std::call_once(g_ipc_env_once, []() {
        RequireEnv("HFT_IPC_SHM_MARKET_DATA", g_market_data_shm);
        RequireEnv("HFT_IPC_SHM_FEEDER_TO_OBSERVER", g_feeder_observer_shm);
        RequireEnv("HFT_IPC_SHM_TRADER_TO_OBSERVER", g_trader_observer_shm);
    });
}

}  // namespace

const char* IpcMarketDataShmName() {
    LoadFromEnvironmentOnce();
    return g_market_data_shm.c_str();
}

const char* IpcFeederToObserverShmName() {
    LoadFromEnvironmentOnce();
    return g_feeder_observer_shm.c_str();
}

const char* IpcTraderToObserverShmName() {
    LoadFromEnvironmentOnce();
    return g_trader_observer_shm.c_str();
}

}  // namespace hft

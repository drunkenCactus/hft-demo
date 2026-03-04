#include <lib/interprocess/interprocess.hpp>

namespace hft {

void HandleMdFeederData(const ObserverRingBufferData& data) {

}

void HandleTradingEngineBtcData(const ObserverRingBufferData& data) {

}

void RunObserver() {
    ShmToObserver shm_from_md_feeder(SHM_NAME_MD_FEEDER_TO_OBSERVER, MemoryRole::OPEN_ONLY);
    ShmToObserver shm_from_trading_engine_btc(SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER, MemoryRole::OPEN_ONLY);
    auto [buffer_from_md_feeder] = shm_from_md_feeder.GetObjects();
    auto [buffer_from_trading_engine_btc] = shm_from_trading_engine_btc.GetObjects();

    ObserverRingBufferData data;
    while (true) {
        if (buffer_from_md_feeder->Read(data, 0)) {
            HandleMdFeederData(data);
        }
        if (buffer_from_trading_engine_btc->Read(data, 0)) {
            HandleTradingEngineBtcData(data);
        }
    }
}

}  // namespace hft

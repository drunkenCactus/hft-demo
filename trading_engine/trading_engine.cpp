#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <thread>

namespace hft {

namespace {

constexpr uint32_t CONSUMER_ID = 0;
constexpr uint32_t RECONNECT_TIMEOUT_MS = 100;
constexpr uint32_t LIVENESS_TRESHOLD_SECONDS = 5;

void DoStrategy(const BestBidAskRingBufferData& market_data) {
    auto [bid_px, bid_qty] = market_data.best_bid;
    auto [ask_px, ask_qty] = market_data.best_ask;
    if (bid_px == 0.0 || ask_px == 0.0) {
        return;
    }
    double spread = ask_px - bid_px;
    if (spread >= 0.2) {
        double buy_price = bid_px + 0.01;
        double sell_price = ask_px - 0.01;
        double order_qty = 0.001;
        HOT_INFO << "BUY {" << buy_price << "," << order_qty << "}" << Endl;
        HOT_INFO << "SELL {" << sell_price << "," << order_qty << "}" << Endl;
    }
}

}  // namespace

int RunTradingEngine() {
    std::unique_ptr<ShmToObserver> shm_log = nullptr;
    try {
        RemoveSharedMemory(SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER);
        shm_log = std::make_unique<ShmToObserver>(SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER, MemoryRole::CREATE_ONLY);
        shm_log->UpdateHeartbeat();
    } catch (const std::exception& e) {
        return 1;
    }

    auto [ring_buffer_log] = shm_log->GetObjects();
    HotPathLogger::Init(ring_buffer_log);
    HOT_INFO << "Trading Engine started!" << Endl;

    std::unique_ptr<ShmMdFeederToTradingEngine> shm_market_data = nullptr;
    while (shm_market_data == nullptr) {
        try {
            shm_market_data = std::make_unique<ShmMdFeederToTradingEngine>(SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE, MemoryRole::OPEN_ONLY);
        } catch (const ShmVersionConflict& e) {
            HOT_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            HOT_WARNING << "Failed to open Market Data shared memory: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_TIMEOUT_MS));
        }
    }
    HOT_INFO << "Market Data shared memory opened" << Endl;

    auto [ring_buffer_md_feeder] = shm_market_data->GetObjects();
    ring_buffer_md_feeder->ResetConsumer(CONSUMER_ID);

    BestBidAskRingBufferData market_data;
    while (true) {
        shm_log->UpdateHeartbeat();

        ReadResult result = ring_buffer_md_feeder->Read(market_data, CONSUMER_ID);
        if (result == ReadResult::SUCCESS) {
            DoStrategy(market_data);
        } else if (result == ReadResult::CONSUMER_IS_DISABLED) {
            HOT_ERROR << "Consumer is disabled. Exiting" << Endl;
            return 1;
        } else if (!shm_market_data->IsProducerAlive(LIVENESS_TRESHOLD_SECONDS)) {
            HOT_ERROR << "Producer is dead. Exiting" << Endl;
            return 1;
        }
    }

    HOT_INFO << "Finishing Trading Engine" << Endl;
    return 0;
}

}  // namespace hft

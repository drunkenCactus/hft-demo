#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <chrono>

constexpr uint32_t CONSUMER_ID = 0;

void DoStrategy(const hft::BestBidAskRingBufferData& shared_data) {
    auto [bid_px, bid_qty] = shared_data.best_bid;
    auto [ask_px, ask_qty] = shared_data.best_ask;
    if (bid_px == 0 || ask_px == 0) {
        return;
    }
    double spread = ask_px - bid_px;
    if (spread >= 0.2) {
        const double buy_price = bid_px + 0.01;
        const double sell_price = ask_px - 0.01;
        const double order_qty = 0.001;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        HOT_INFO << "Current ts " << now << ", data ts " << shared_data.ts << Endl;
        HOT_INFO << "BUY{" << buy_price << "," << order_qty << "}" << Endl;
        HOT_INFO << "SELL{" << sell_price << "," << order_qty << "}" << Endl;
    }
}

int main(int argc, char *argv[]) {
    try {
        hft::RemoveSharedMemory(hft::SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER);
        hft::ShmToObserver shm_log(hft::SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER, hft::MemoryRole::CREATE_ONLY);
        auto [ring_buffer_log] = shm_log.GetObjects();

        hft::HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "Trading Engine started!" << Endl;

        hft::ShmMdFeederToTradingEngine shared_memory(hft::SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE, hft::MemoryRole::OPEN_ONLY);
        auto [ring_buffer] = shared_memory.GetObjects();

        ring_buffer->ResetConsumer(CONSUMER_ID);

        while (true) {
            hft::BestBidAskRingBufferData shared_data;
            if (ring_buffer->Read(shared_data, CONSUMER_ID)) {
                DoStrategy(shared_data);
            }
        }

    } catch (const std::exception& e) {
        HOT_ERROR << "Exception: " << e.what() << Endl;
        return 1;
    }

    return 0;
}

#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <thread>

namespace hft {

namespace {

constexpr uint32_t CONSUMER_ID = 0;
constexpr uint32_t RECONNECT_TIMEOUT_MS = 100;
constexpr uint32_t LIVENESS_TRESHOLD_SECONDS = 5;

}  // namespace

int RunTrader() {
    std::unique_ptr<ShmToObserver> shm_log = nullptr;
    try {
        RemoveSharedMemory(SHM_NAME_TRADER_TO_OBSERVER);
        shm_log = std::make_unique<ShmToObserver>(SHM_NAME_TRADER_TO_OBSERVER, MemoryRole::CREATE_ONLY);
        shm_log->UpdateHeartbeat();
    } catch (const std::exception& e) {
        return 1;
    }

    auto [ring_buffer_log] = shm_log->GetObjects();
    HotPathLogger::Init(ring_buffer_log);
    HOT_INFO << "Trader started!" << Endl;

    std::unique_ptr<ShmMarketData> shm_market_data = nullptr;
    while (shm_market_data == nullptr) {
        try {
            shm_market_data = std::make_unique<ShmMarketData>(SHM_NAME_MARKET_DATA, MemoryRole::OPEN_ONLY);
        } catch (const ShmVersionConflict& e) {
            HOT_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            HOT_WARNING << "Failed to open Market Data shared memory: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_TIMEOUT_MS));
        }
    }
    HOT_INFO << "Market Data shared memory opened" << Endl;

    auto [rb_order_book_updates, rb_trades, order_book_snp] = shm_market_data->GetObjects();
    rb_order_book_updates->ResetConsumer(CONSUMER_ID);
    rb_trades->ResetConsumer(CONSUMER_ID);

    auto has_ring_buffer_reading_error = [&shm_market_data](ReadResult result) {
        if (result == ReadResult::CONSUMER_IS_DISABLED) {
            HOT_ERROR << "Consumer is disabled. Exiting" << Endl;
            return true;
        } else if (!shm_market_data->IsProducerAlive(LIVENESS_TRESHOLD_SECONDS)) {
            HOT_ERROR << "Producer is dead. Exiting" << Endl;
            return true;
        }
        return false;
    };

    OrderBookUpdate order_book_update;
    Trade trade;

    while (true) {
        shm_log->UpdateHeartbeat();
        {
            ReadResult result = rb_order_book_updates->Read(order_book_update, CONSUMER_ID);
            if (result == ReadResult::SUCCESS) {
                HOT_INFO << "DEPTH type:" << static_cast<uint8_t>(order_book_update.type)
                         << "; price:" << order_book_update.price
                         << "; quantity:" << order_book_update.quantity << Endl;
            } else if (has_ring_buffer_reading_error(result)) {
                return 1;
            }
        }
        {
            ReadResult result = rb_trades->Read(trade, CONSUMER_ID);
            if (result == ReadResult::SUCCESS) {
                HOT_INFO << "TRADE price:" << trade.price << "; quantity:" << trade.quantity << Endl;
            } else if (has_ring_buffer_reading_error(result)) {
                return 1;
            }
        }
    }

    HOT_INFO << "Finishing Trader" << Endl;
    return 0;
}

}  // namespace hft

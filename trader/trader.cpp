#include <lib/binance/binance_api_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <thread>

namespace hft {

namespace {

constexpr uint32_t CONSUMER_ID = 0;
constexpr uint32_t RECONNECT_TIMEOUT_MS = 100;
constexpr uint32_t LIVENESS_TRESHOLD_SECONDS = 5;

class OrderBookProcessor {
public:
    OrderBookProcessor(OrderBookUpdateRingBuffer* buffer)
        : buffer_(buffer)
        , binance_api_client_("BTCUSDT", ORDER_BOOK_DEPTH)
    {}

    [[nodiscard]] bool ProcessUpdate() {
        OrderBookUpdate update;
        ReadResult result = buffer_->Read(update, CONSUMER_ID);
        if (result == ReadResult::SUCCESS) {
            if (update.first_update_id > snapshot_.last_update_id + 1) {
                // order_book is corrupted
                HOT_WARNING << "Order book snapshot is corrupted" << Endl;
                if (!ResetSnapshot()) {
                    return false;
                }
            } else if (update.last_update_id < snapshot_.last_update_id) {
                // event is too old
                HOT_WARNING << "Order book update is too old. Skip it" << Endl;
            } else {
                // update local order book
                snapshot_.last_update_id = update.last_update_id;
            }
        } else if (result == ReadResult::CONSUMER_IS_DISABLED) {
            HOT_ERROR << "Consumer for order book updates is disabled" << Endl;
            return false;
        }
        return true;
    }

private:
    [[nodiscard]] bool ResetSnapshot() {
        std::string_view response = binance_api_client_.GetOrderBookShapshot();
        if (!ParseOrderBookSnapshot(
            response,
            [this](const OrderBookSnapshot& value) {
                snapshot_ = value;
            }
        )) {
            HOT_ERROR << "Order book snapshot parsing error" << Endl;
            return false;
        }
        HOT_INFO << "Order book snapshot resetted successfully" << Endl;
        return true;
    }

private:
    OrderBookUpdateRingBuffer* buffer_;
    BinanceApiClient binance_api_client_;
    OrderBookSnapshot snapshot_;
};

class TradeProcessor {
public:
    TradeProcessor(TradeRingBuffer* buffer)
        : buffer_(buffer)
    {}

    [[nodiscard]] bool ProcessTrade() {
        Trade trade;
        ReadResult result = buffer_->Read(trade, CONSUMER_ID);
        if (result == ReadResult::SUCCESS) {
            // process trade
        } else if (result == ReadResult::CONSUMER_IS_DISABLED) {
            HOT_ERROR << "Consumer for trades is disabled" << Endl;
            return false;
        }
        return true;
    }

private:
    TradeRingBuffer* buffer_;
};

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

    BinanceApiClient binance_api_client("BTCUSDT", ORDER_BOOK_DEPTH);

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

    auto [rb_order_book_updates, rb_trades] = shm_market_data->GetObjects();
    OrderBookProcessor order_book_processor(rb_order_book_updates);
    TradeProcessor trade_processor(rb_trades);

    rb_order_book_updates->ResetConsumer(CONSUMER_ID);
    rb_trades->ResetConsumer(CONSUMER_ID);

    while (true) {
        if (!shm_market_data->IsProducerAlive(LIVENESS_TRESHOLD_SECONDS)) {
            HOT_ERROR << "Producer is dead" << Endl;
            return 1;
        }
        shm_log->UpdateHeartbeat();
        if (!order_book_processor.ProcessUpdate()) {
            return 1;
        }
        if (!trade_processor.ProcessTrade()) {
            return 1;
        }
    }

    HOT_INFO << "Finishing Trader" << Endl;
    return 0;
}

}  // namespace hft

#include <lib/binance/binance_api_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/trade_flow_window.hpp>

#include <span>
#include <thread>

namespace hft {

namespace {

constexpr uint32_t kConsumerId = 0;
constexpr uint32_t kReconnectTimeoutMs = 100;
constexpr uint32_t kLivenessThresholdSeconds = 5;

class OrderBookProcessor {
public:
    OrderBookProcessor(OrderBookUpdateRingBuffer* buffer)
        : buffer_(buffer)
        , binance_api_client_("BTCUSDT", kOrderBookDepth)
    {}

    [[nodiscard]] bool ProcessUpdate() {
        OrderBookUpdate update;
        ReadResult result = buffer_->Read(update, kConsumerId);
        if (result == ReadResult::kSuccess) {
            if (update.first_update_id > order_book_.LastUpdateId() + 1) {
                // order_book is corrupted
                HOT_WARNING << "Order book snapshot is corrupted" << Endl;
                if (!ResetSnapshot()) {
                    return false;
                }
            } else if (update.last_update_id < order_book_.LastUpdateId()) {
                // event is too old
                HOT_WARNING << "Order book update is too old. Skip it" << Endl;
            } else {
                // update local order book
                if (update.type == OrderBookUpdate::Type::kBid) {
                    order_book_.UpdateBid(update.last_update_id, update.price, update.quantity);
                } else {
                    order_book_.UpdateAsk(update.last_update_id, update.price, update.quantity);
                }
            }
        } else if (result == ReadResult::kConsumerIsDisabled) {
            HOT_ERROR << "Consumer for order book updates is disabled" << Endl;
            return false;
        }
        return true;
    }

    const OrderBook& Book() const noexcept {
        return order_book_;
    }

private:
    [[nodiscard]] bool ResetSnapshot() {
        std::string_view response = binance_api_client_.GetOrderBookSnapshot();
        if (!ParseOrderBookSnapshot(
            response,
            [this](const OrderBookSnapshot& snapshot) {
                order_book_.Init(
                    snapshot.last_update_id,
                    snapshot.bids_prices,
                    snapshot.bids_quantities,
                    snapshot.bids_depth,
                    snapshot.asks_prices,
                    snapshot.asks_quantities,
                    snapshot.asks_depth
                );
            }
        )) {
            HOT_ERROR << "Order book snapshot parsing error" << Endl;
            return false;
        } else {
            HOT_INFO << "Order book snapshot was reset successfully" << Endl;
            return true;
        }
    }

private:
    OrderBookUpdateRingBuffer* buffer_;
    BinanceApiClient binance_api_client_;
    OrderBook order_book_;
};

class TradeProcessor {
public:
    TradeProcessor(TradeRingBuffer* buffer)
        : buffer_(buffer)
    {}

    [[nodiscard]] bool ProcessTrade() {
        Trade trade;
        ReadResult result = buffer_->Read(trade, kConsumerId);
        if (result == ReadResult::kSuccess) {
            flow_window_.OnTrade(
                trade.meta.event_timestamp_microseconds,
                trade.is_buyer_maker,
                trade.quantity
            );
        } else if (result == ReadResult::kConsumerIsDisabled) {
            HOT_ERROR << "Consumer for trades is disabled" << Endl;
            return false;
        }
        return true;
    }

    const TradeFlowWindow& FlowWindow() const noexcept {
        return flow_window_;
    }

private:
    TradeRingBuffer* buffer_;
    TradeFlowWindow flow_window_;
};

class OrderProcessor {
public:
    OrderProcessor(const OrderBook& book, const TradeFlowWindow& flow)
        : book_(book)
        , flow_(flow)
    {}

    [[nodiscard]] bool CreateOrder(Order& order) noexcept {
        const auto& best_bid = book_.GetBestBid();
        const auto& best_ask = book_.GetBestAsk();
        if (best_bid.price == 0 || best_ask.price == 0 || best_bid.price >= best_ask.price) {
            return false;
        }

        const uint64_t bid_sum = SumQuantities(book_.GetTopBids(kDepthLevels));
        const uint64_t ask_sum = SumQuantities(book_.GetTopAsks(kDepthLevels));

        // 9*bid > 11*ask ~ >55% bid share
        const bool bid_heavy = (ask_sum == 0 && bid_sum > 0) || (9ULL * bid_sum > 11ULL * ask_sum);
        const bool ask_heavy = (bid_sum == 0 && ask_sum > 0) || (9ULL * ask_sum > 11ULL * bid_sum);

        const bool buy_heavy = flow_.AggressiveBuyVolume() > flow_.AggressiveSellVolume();
        const bool sell_heavy = flow_.AggressiveSellVolume() > flow_.AggressiveBuyVolume();

        if (bid_heavy && buy_heavy) {
            order.type = Order::Type::kBuy;
            order.price = best_ask.price;
            order.quantity = kOrderQty;
        } else if (ask_heavy && sell_heavy) {
            order.type = Order::Type::kSell;
            order.price = best_bid.price;
            order.quantity = kOrderQty;
        } else {
            return false;
        }

        if (last_order_.type == order.type && last_order_.price == order.price) {
            return false;
        }
        last_order_ = order;
        return true;
    }

private:
    static uint64_t SumQuantities(std::span<const OrderBookRow> levels) noexcept {
        uint64_t sum = 0;
        for (const OrderBookRow& row : levels) {
            sum += row.quantity;
        }
        return sum;
    }

private:
    const OrderBook& book_;
    const TradeFlowWindow& flow_;
    static constexpr uint32_t kDepthLevels = 5;
    static constexpr uint64_t kOrderQty = 10;

    Order last_order_;
};

}  // namespace

int RunTrader() {
    std::unique_ptr<ShmToObserver> shm_log = nullptr;
    try {
        RemoveSharedMemory(kShmNameTraderToObserver);
        shm_log = std::make_unique<ShmToObserver>(kShmNameTraderToObserver, MemoryRole::kCreateOnly);
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
            shm_market_data = std::make_unique<ShmMarketData>(kShmNameMarketData, MemoryRole::kOpenOnly);
        } catch (const ShmVersionConflict& e) {
            HOT_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            HOT_WARNING << "Failed to open Market Data shared memory: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(kReconnectTimeoutMs));
        }
    }
    HOT_INFO << "Market Data shared memory opened" << Endl;

    auto [rb_order_book_updates, rb_trades] = shm_market_data->GetObjects();
    OrderBookProcessor order_book_processor(rb_order_book_updates);
    TradeProcessor trade_processor(rb_trades);
    OrderProcessor order_processor(order_book_processor.Book(), trade_processor.FlowWindow());

    rb_order_book_updates->ResetConsumer(kConsumerId);
    rb_trades->ResetConsumer(kConsumerId);

    Order order;
    while (true) {
        if (!shm_market_data->IsProducerAlive(kLivenessThresholdSeconds)) {
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
        if (order_processor.CreateOrder(order)) {
            HOT_INFO << "Created order: " << (order.type == Order::Type::kBuy ? "BUY" : "SELL")
                     << " price=" << order.price << " qty=" << order.quantity << Endl;
        }
    }

    HOT_INFO << "Finishing Trader" << Endl;
    return 0;
}

}  // namespace hft

#include <lib/binance/binance_ws_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_params.hpp>
#include <lib/interprocess/ring_buffer_data.hpp>

#include <memory>
#include <optional>

namespace hft {

namespace {

constexpr const char* kHost = "stream.binance.com";
constexpr const char* kPort = "9443";
constexpr const char* kTarget = "/stream?streams=btcusdt@depth@100ms/btcusdt@trade/ethusdt@depth@100ms/ethusdt@trade&timeUnit=microsecond";

struct MarketDataBuffers {
    OrderBookUpdateRingBuffer* order_book_updates = nullptr;
    TradeRingBuffer* trades = nullptr;
};

constexpr std::optional<TraderId> TraderIdForSymbol(Symbol symbol) noexcept {
    switch (symbol) {
        case Symbol::kBtcUsdt:
            return TraderId::kBtcUsdt;
        case Symbol::kEthUsdt:
            return TraderId::kEthUsdt;
        default:
            return std::nullopt;
    }
}

}  // namespace

int RunFeeder() {
    try {
        RemoveSharedMemory(IpcFeederToObserverShmName());
        ShmToObserver shm_log(IpcFeederToObserverShmName(), MemoryRole::kCreateOnly);
        shm_log.UpdateHeartbeat();
        auto [ring_buffer_log] = shm_log.GetObjects();

        HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "Feeder started!" << Endl;

        ArrayByTraderId<std::unique_ptr<ShmMarketData>> md_shm{};
        ArrayByTraderId<MarketDataBuffers> md_buffers{};

        for (TraderId id : kTraderIds) {
            const TraderConfig& cfg = GetTraderConfig(id);
            RemoveSharedMemory(cfg.market_data_shm.c_str());
            md_shm[id] = std::make_unique<ShmMarketData>(cfg.market_data_shm.c_str(), MemoryRole::kCreateOnly);
            md_shm[id]->UpdateHeartbeat();
            auto [rb_order_book_updates, rb_trades] = md_shm[id]->GetObjects();
            md_buffers[id].order_book_updates = rb_order_book_updates;
            md_buffers[id].trades = rb_trades;
            HOT_INFO << "Market Data shm created: " << cfg.market_data_shm.c_str() << Endl;
        }

        BinanceWsClient client(kHost, kPort, kTarget);
        HOT_INFO << "Binance websocket connected" << Endl;

        auto order_book_update_callback = [&md_buffers](const OrderBookUpdate& order_book_update) {
            if (const std::optional<TraderId> id = TraderIdForSymbol(order_book_update.symbol); id.has_value()) {
                md_buffers[id.value()].order_book_updates->Write(order_book_update);
            }
        };

        auto trade_callback = [&md_buffers](const Trade& trade) {
            if (const std::optional<TraderId> id = TraderIdForSymbol(trade.symbol); id.has_value()) {
                md_buffers[id.value()].trades->Write(trade);
            }
        };

        while (true) {
            shm_log.UpdateHeartbeat();
            for (TraderId id : kTraderIds) {
                md_shm[id]->UpdateHeartbeat();
            }

            std::string_view message = client.Read();
            if (!ParseEvent(message, order_book_update_callback, trade_callback)) {
                HOT_WARNING << "Parsing error" << Endl;
            }
        }

    } catch (const std::exception& e) {
        HOT_ERROR << "Exception: " << e.what() << Endl;
        return 1;
    }

    HOT_INFO << "Finishing Feeder" << Endl;
    return 0;
}

}  // namespace hft

#include <lib/binance/binance_ws_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_params.hpp>
#include <lib/interprocess/ring_buffer_data.hpp>
#include <lib/interprocess/trader_id.hpp>

#include <array>
#include <memory>

namespace hft {

namespace {

constexpr const char* kHost = "stream.binance.com";
constexpr const char* kPort = "9443";
constexpr const char* kTarget = "/stream?streams=btcusdt@depth@100ms/btcusdt@trade/ethusdt@depth@100ms/ethusdt@trade&timeUnit=microsecond";

struct MarketDataBuffers {
    OrderBookUpdateRingBuffer* order_book_updates = nullptr;
    TradeRingBuffer* trades = nullptr;
};

}  // namespace

int RunFeeder() {
    try {
        RemoveSharedMemory(IpcFeederToObserverShmName());
        ShmToObserver shm_log(IpcFeederToObserverShmName(), MemoryRole::kCreateOnly);
        shm_log.UpdateHeartbeat();
        auto [ring_buffer_log] = shm_log.GetObjects();

        HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "Feeder started!" << Endl;

        std::array<std::unique_ptr<ShmMarketData>, kTraderCount> md_shm;
        std::array<MarketDataBuffers, kTraderCount> md_buffers;

        for (std::size_t i = 0; i < kTraderCount; ++i) {
            const TraderConfig& cfg = GetTraderConfig(static_cast<TraderId>(i));
            RemoveSharedMemory(cfg.market_data_shm.c_str());
            md_shm[i] = std::make_unique<ShmMarketData>(cfg.market_data_shm.c_str(), MemoryRole::kCreateOnly);
            md_shm[i]->UpdateHeartbeat();
            auto [rb_order_book_updates, rb_trades] = md_shm[i]->GetObjects();
            md_buffers[i].order_book_updates = rb_order_book_updates;
            md_buffers[i].trades = rb_trades;
            HOT_INFO << "Market Data shm created: " << cfg.market_data_shm.c_str() << Endl;
        }

        BinanceWsClient client(kHost, kPort, kTarget);
        HOT_INFO << "Binance websocket connected" << Endl;

        auto order_book_update_callback = [&md_buffers](const OrderBookUpdate& order_book_update) {
            switch (order_book_update.symbol) {
                case Symbol::kBtcUsdt:
                    md_buffers[TraderIdToIndex(TraderId::kBtcUsdt)].order_book_updates->Write(order_book_update);
                    break;
                case Symbol::kEthUsdt:
                    md_buffers[TraderIdToIndex(TraderId::kEthUsdt)].order_book_updates->Write(order_book_update);
                    break;
                default:
                    break;
            }
        };

        auto trade_callback = [&md_buffers](const Trade& trade) {
            switch (trade.symbol) {
                case Symbol::kBtcUsdt:
                    md_buffers[TraderIdToIndex(TraderId::kBtcUsdt)].trades->Write(trade);
                    break;
                case Symbol::kEthUsdt:
                    md_buffers[TraderIdToIndex(TraderId::kEthUsdt)].trades->Write(trade);
                    break;
                default:
                    break;
            }
        };

        while (true) {
            shm_log.UpdateHeartbeat();
            for (std::size_t i = 0; i < kTraderCount; ++i) {
                md_shm[i]->UpdateHeartbeat();
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

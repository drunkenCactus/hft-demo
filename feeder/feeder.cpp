#include <lib/binance/binance_ws_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_env.hpp>

#include <rapidjson/document.h>

namespace hft {

namespace {

constexpr const char* kHost = "stream.binance.com";
constexpr const char* kPort = "9443";
constexpr const char* kTarget = "/stream?streams=btcusdt@depth@100ms/btcusdt@trade&timeUnit=microsecond";

}  // namespace

int RunFeeder() {
    try {
        RemoveSharedMemory(IpcFeederToObserverShmName());
        ShmToObserver shm_log(IpcFeederToObserverShmName(), MemoryRole::kCreateOnly);
        shm_log.UpdateHeartbeat();
        auto [ring_buffer_log] = shm_log.GetObjects();

        HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "Feeder started!" << Endl;

        RemoveSharedMemory(IpcMarketDataShmName());
        ShmMarketData shm_market_data(IpcMarketDataShmName(), MemoryRole::kCreateOnly);
        shm_market_data.UpdateHeartbeat();
        auto [rb_order_book_updates, rb_trades] = shm_market_data.GetObjects();
        HOT_INFO << "Market Data shared memory created" << Endl;

        BinanceWsClient client(kHost, kPort, kTarget);
        HOT_INFO << "Binance websocket connected" << Endl;

        auto order_book_update_callback = [rb_order_book_updates](const OrderBookUpdate& order_book_update) {
            rb_order_book_updates->Write(order_book_update);
        };

        auto trade_callback = [rb_trades](const Trade& trade) {
            rb_trades->Write(trade);
        };

        while (true) {
            shm_log.UpdateHeartbeat();
            shm_market_data.UpdateHeartbeat();

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

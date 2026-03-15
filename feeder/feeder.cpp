#include <lib/binance/binance_ws_client.hpp>
#include <lib/binance/parser.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <rapidjson/document.h>

namespace hft {

namespace {

constexpr const char* HOST = "stream.binance.com";
constexpr const char* PORT = "9443";
constexpr const char* TARGET = "/stream?streams=btcusdt@depth@100ms/btcusdt@trade&timeUnit=microsecond";

}  // namespace

int RunFeeder() {
    try {
        RemoveSharedMemory(SHM_NAME_FEEDER_TO_OBSERVER);
        ShmToObserver shm_log(SHM_NAME_FEEDER_TO_OBSERVER, MemoryRole::CREATE_ONLY);
        shm_log.UpdateHeartbeat();
        auto [ring_buffer_log] = shm_log.GetObjects();

        HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "Feeder started!" << Endl;

        RemoveSharedMemory(SHM_NAME_MARKET_DATA);
        ShmMarketData shm_market_data(SHM_NAME_MARKET_DATA, MemoryRole::CREATE_ONLY);
        shm_market_data.UpdateHeartbeat();
        auto [rb_order_book_updates, rb_trades] = shm_market_data.GetObjects();
        HOT_INFO << "Market Data shared memory created" << Endl;

        BinanceWsClient client(HOST, PORT, TARGET);
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

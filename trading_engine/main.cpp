#include <lib/interprocess.hpp>
#include <lib/logger.hpp>
#include <lib/shared_memory.hpp>

#include <chrono>

const std::string LOGFILE_PATH = "/var/log/hft/trading_engine.log";
constexpr uint32_t CONSUMER_ID = 0;

Logger<LogLevel::INFO> LOG_INFO(LOGFILE_PATH);
Logger<LogLevel::ERROR> LOG_ERROR(LOGFILE_PATH);

void DoStrategy(const hft::BestBidAskData& shared_data) {
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

        LOG_INFO << "Current ts " << now << ", data ts " << shared_data.ts << Endl;
        LOG_INFO << "BUY{" << buy_price << "," << order_qty << "}" << Endl;
        LOG_INFO << "SELL{" << sell_price << "," << order_qty << "}" << Endl;
    }
}

int main(int argc, char *argv[]) {
    ProcessWatcher watcher(LOG_INFO, "Trading Engine");

    try {
        hft::SharedMemory<
            hft::MemoryRole::OPEN_ONLY,
            hft::CACHE_LINE_SIZE,
            hft::BestBidAskRingBuffer
        > shared_memory(hft::SHARED_MEMORY_NAME);
        auto [ring_buffer] = shared_memory.GetObjects();

        ring_buffer->ResetConsumer(CONSUMER_ID);

        while (true) {
            hft::BestBidAskData shared_data;
            if (ring_buffer->Read(shared_data, CONSUMER_ID)) {
                DoStrategy(shared_data);
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR << "Exception: " << e.what() << Endl;
        return 1;
    }

    return 0;
}

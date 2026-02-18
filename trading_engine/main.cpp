#include <boost/interprocess/detail/os_file_functions.hpp>
#include <lib/logger.hpp>
#include <lib/shared_memory.hpp>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <chrono>
#include <thread>

const std::string LOGFILE_PATH = "/var/log/hft/trading_engine.log";

Logger<LogLevel::INFO> LOG_INFO(LOGFILE_PATH);
Logger<LogLevel::ERROR> LOG_ERROR(LOGFILE_PATH);

void DoStrategy(const hft::SharedData& shared_data) {
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
    LOG_INFO << "Trading Engine started!" << Endl;

    boost::interprocess::shared_memory_object shared_memory(
        boost::interprocess::open_only,
        hft::SHARED_MEMORY_NAME,
        boost::interprocess::read_only
    );

    boost::interprocess::mapped_region region(shared_memory, boost::interprocess::read_only);
    const hft::SharedData* shared_data = static_cast<const hft::SharedData*>(region.get_address());

    while (true) {
        DoStrategy(*shared_data);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    LOG_INFO << "Trading Engine stopped!" << Endl;
    return 0;
}

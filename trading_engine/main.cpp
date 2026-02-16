#include <lib/logger.hpp>

#include <chrono>
#include <thread>

const std::string LOGFILE_PATH = "/var/log/hft/trading_engine.log";

Logger<LogLevel::INFO> LOG_INFO(LOGFILE_PATH);
Logger<LogLevel::ERROR> LOG_ERROR(LOGFILE_PATH);

int main(int argc, char *argv[]) {
    LOG_INFO << "Trading Engine started!" << Endl;
    uint64_t seconds = 0;
    while (true) {
        LOG_INFO << "Alive for " << ++seconds << " seconds" << Endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    LOG_INFO << "Trading Engine stopped!" << Endl;
    return 0;
}

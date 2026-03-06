#include <lib/interprocess/interprocess.hpp>
#include <lib/logger.hpp>

#include <fstream>
#include <thread>

namespace hft {

namespace {

const std::string LOGFILES_DIR = "/var/log/hft/";
const std::string OBSERVER_LOGFILE_PATH = LOGFILES_DIR + "observer.log";

const std::vector<std::pair<const char* const, std::string>> SHM_NAME_TO_LOGFILE_PATH = {
    {SHM_NAME_MD_FEEDER_TO_OBSERVER, LOGFILES_DIR + "md_feeder.log"},
    {SHM_NAME_TRADING_ENGINE_BTC_TO_OBSERVER, LOGFILES_DIR + "trading_engine_btc.log"},
};

constexpr uint32_t RECONNECT_TIMEOUT_SECONDS = 1;
constexpr uint32_t LIVENESS_TRESHOLD_SECONDS = 5;

int ProcessLogAttempt(const char* const shm_name, std::ofstream& logfile) {
    std::unique_ptr<ShmToObserver> shm = nullptr;
    while (shm == nullptr) {
        try {
            shm = std::make_unique<ShmToObserver>(shm_name, MemoryRole::OPEN_ONLY);
        } catch (const ShmVersionConflict& e) {
            LOG_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            LOG_WARNING << "Failed to open {" << shm_name << "}: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_TIMEOUT_SECONDS));
        }
    }
    auto [ring_buffer] = shm->GetObjects();
    LOG_INFO << "Start receiving logs from {" << shm_name << "}" << Endl;

    ObserverRingBufferData data;
    while (true) {
        ReadResult result = ring_buffer->Read(data);
        if (result == ReadResult::SUCCESS) {
            const auto time = std::chrono::system_clock::time_point(std::chrono::nanoseconds(data.timestamp_ns));
            WriteLog(logfile, time, data.level, data.message);
        } else if (result == ReadResult::CONSUMER_IS_DISABLED) {
            LOG_WARNING << "Consumer for {" << shm_name << "} is disabled" << Endl;
            ring_buffer->ResetConsumer();
        } else if (!shm->IsProducerAlive(LIVENESS_TRESHOLD_SECONDS)) {
            LOG_WARNING << "Producer for {" << shm_name << "} is dead" << Endl;
            return 0;
        }
    }
    return 0;
}

void ProcessLog(const char* const shm_name, const std::string& logfile_path) {
    std::ofstream logfile;
    logfile.open(logfile_path, std::ios::app);
    if (!logfile.is_open()) {
        LOG_ERROR << "Сannot open logfile " << logfile_path << Endl;
        std::exit(1);
    }

    while (true) {
        if (ProcessLogAttempt(shm_name, logfile) != 0) {
            logfile.close();
            std::exit(1);
        }
        std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_TIMEOUT_SECONDS));
        LOG_WARNING << "Restarting logging for {" << shm_name << "}" << Endl;
    }

    LOG_ERROR << "Unexpected event" << Endl;
    logfile.close();
}

}  // namespace

void RunObserver() {
    Logger::Init(OBSERVER_LOGFILE_PATH);
    LOG_INFO << "Observer started!" << Endl;

    std::vector<std::thread> threads;
    threads.reserve(SHM_NAME_TO_LOGFILE_PATH.size());
    for (const auto& [shm_name, logfile_path] : SHM_NAME_TO_LOGFILE_PATH) {
        threads.emplace_back(ProcessLog, shm_name, logfile_path);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    LOG_INFO << "Finishing Observer" << Endl;
}

}  // namespace hft

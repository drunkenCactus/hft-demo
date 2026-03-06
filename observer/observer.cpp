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

class FailedToOpenLogfile : public std::runtime_error {
public:
    FailedToOpenLogfile(const std::string& logfile_path)
        : std::runtime_error("Сannot open logfile " + logfile_path) {}
};

class LogProcessor {
public:
    LogProcessor(const char* const shm_name, const std::string& logfile_path)
        : shared_memory_(shm_name, MemoryRole::OPEN_ONLY)
        , ring_buffer_(std::get<ObserverRingBuffer*>(shared_memory_.GetObjects()))
    {
        logfile_.open(logfile_path, std::ios::app);
        if (!logfile_.is_open()) {
            throw FailedToOpenLogfile(logfile_path);
        }
    }

    LogProcessor(const LogProcessor& other) = delete;
    LogProcessor(LogProcessor&& other) = delete;
    LogProcessor& operator=(const LogProcessor& other) = delete;
    LogProcessor& operator=(LogProcessor&& other) = delete;

    ~LogProcessor() {
        logfile_.close();
    }

    void Run() {
        ObserverRingBufferData data;
        while (true) {
            if (ring_buffer_->Read(data)) {
                const auto time = std::chrono::system_clock::time_point(std::chrono::nanoseconds(data.timestamp_ns));
                WriteLog(logfile_, time, data.level, data.message);
            }
        }
    }

private:
    ShmToObserver shared_memory_;
    ObserverRingBuffer* ring_buffer_ = nullptr;
    std::ofstream logfile_;
};

void ProcessLog(const char* const shm_name, const std::string& logfile_path) {
    std::unique_ptr<LogProcessor> processor = nullptr;
    while (processor == nullptr) {
        try {
            processor = std::make_unique<LogProcessor>(shm_name, logfile_path);
        } catch (const FailedToOpenLogfile& e) {
            LOG_ERROR << e.what() << Endl;
            return;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to open shared memory " << shm_name << Endl;
            std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_TIMEOUT_SECONDS));
        }
    }
    LOG_INFO << "Start receiving logs from " << shm_name << Endl;
    processor->Run();
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
}

}  // namespace hft

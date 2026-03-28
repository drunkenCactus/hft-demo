#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_env.hpp>
#include <lib/logger.hpp>

#include <fstream>
#include <thread>
#include <vector>

namespace hft {

namespace {

const std::string kLogfilesDir = "/var/log/hft/";
const std::string kObserverLogfilePath = kLogfilesDir + "observer.log";

constexpr uint32_t kReconnectTimeoutSeconds = 1;
constexpr uint32_t kLivenessThresholdSeconds = 5;

int ProcessLogAttempt(const char* const shm_name, std::ofstream& logfile) {
    std::unique_ptr<ShmToObserver> shm = nullptr;
    while (shm == nullptr) {
        try {
            shm = std::make_unique<ShmToObserver>(shm_name, MemoryRole::kOpenOnly);
        } catch (const ShmVersionConflict& e) {
            LOG_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            LOG_WARNING << "Failed to open {" << shm_name << "}: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::seconds(kReconnectTimeoutSeconds));
        }
    }
    auto [ring_buffer] = shm->GetObjects();
    LOG_INFO << "Start receiving logs from {" << shm_name << "}" << Endl;

    ObserverData data;
    while (true) {
        ReadResult result = ring_buffer->Read(data);
        if (result == ReadResult::kSuccess) {
            const auto time = std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::nanoseconds(data.timestamp_ns)
                )
            );
            WriteLog(logfile, time, data.level, data.message);
        } else if (result == ReadResult::kConsumerIsDisabled) {
            LOG_WARNING << "Consumer for {" << shm_name << "} is disabled" << Endl;
            ring_buffer->ResetConsumer();
        } else if (!shm->IsProducerAlive(kLivenessThresholdSeconds)) {
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
        LOG_ERROR << "Cannot open logfile " << logfile_path << Endl;
        std::exit(1);
    }

    while (true) {
        if (ProcessLogAttempt(shm_name, logfile) != 0) {
            logfile.close();
            std::exit(1);
        }
        std::this_thread::sleep_for(std::chrono::seconds(kReconnectTimeoutSeconds));
        LOG_WARNING << "Restarting logging for {" << shm_name << "}" << Endl;
    }
}

}  // namespace

void RunObserver() {
    Logger::Init(kObserverLogfilePath);
    LOG_INFO << "Observer started!" << Endl;

    const std::vector<std::pair<std::string, std::string>> log_routes = {
        {IpcFeederToObserverShmName(), kLogfilesDir + "feeder.log"},
        {IpcTraderToObserverShmName(), kLogfilesDir + "trader.log"},
    };

    std::vector<std::thread> threads;
    threads.reserve(log_routes.size());
    for (const auto& [shm_name, logfile_path] : log_routes) {
        threads.emplace_back(ProcessLog, shm_name.c_str(), logfile_path);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    LOG_INFO << "Finishing Observer" << Endl;
}

}  // namespace hft

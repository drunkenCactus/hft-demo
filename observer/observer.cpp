#include <lib/common.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_params.hpp>
#include <lib/logger.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace hft {

namespace {

const std::string kLogfilesDir = "/var/log/hft/";
const std::string kObserverLogfilePath = kLogfilesDir + "observer.log";
const std::string kLatencyCsvPath = kLogfilesDir + "latency_percentiles.csv";

constexpr uint32_t kReconnectTimeoutSeconds = 1;
constexpr uint32_t kLivenessThresholdSeconds = 5;

constexpr std::size_t kLatencyBatchSize = 1000;

uint64_t PercentileFromSorted(const std::vector<uint64_t>& sorted, uint32_t perc) noexcept {
    if (sorted.size() == 0) {
        return 0;
    }
    std::size_t idx = (sorted.size() - 1) * perc / 100;
    return sorted[idx];
}

constexpr const char* GetTraderLogfilePath(TraderId id) noexcept {
    switch (id) {
        case TraderId::kBtcUsdt:
            return "trader_btcusdt.log";
        case TraderId::kEthUsdt:
            return "trader_ethusdt.log";
    }
    std::abort();
}

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

int ProcessLatencyAggregationAttempt(std::ofstream& latency_csv) {
    std::unique_ptr<ShmLatency> shm = nullptr;
    while (shm == nullptr) {
        try {
            shm = std::make_unique<ShmLatency>(IpcLatencyShmName(), MemoryRole::kOpenOnly);
        } catch (const ShmVersionConflict& e) {
            LOG_ERROR << e.what() << Endl;
            return 1;
        } catch (const std::exception& e) {
            LOG_WARNING << "Failed to open latency shm: " << e.what() << Endl;
            std::this_thread::sleep_for(std::chrono::seconds(kReconnectTimeoutSeconds));
        }
    }
    auto [ring_buffer] = shm->GetObjects();
    LOG_INFO << "Start latency aggregation (" << kLatencyBatchSize << " samples per batch) -> " << kLatencyCsvPath << Endl;

    std::vector<uint64_t> batch;
    batch.reserve(kLatencyBatchSize);

    while (true) {
        while (batch.size() < kLatencyBatchSize) {
            LatencyNsSample sample;
            ReadResult result = ring_buffer->Read(sample);
            if (result == ReadResult::kSuccess) {
                batch.push_back(sample.value);
            } else if (result == ReadResult::kConsumerIsDisabled) {
                LOG_WARNING << "Latency consumer is disabled; reset and drop partial batch" << Endl;
                batch.clear();
                ring_buffer->ResetConsumer();
            } else if (!shm->IsProducerAlive(kLivenessThresholdSeconds)) {
                LOG_WARNING << "Latency producer is dead" << Endl;
                return 0;
            }
        }

        std::sort(batch.begin(), batch.end());
        const uint64_t p50 = PercentileFromSorted(batch, 50);
        const uint64_t p90 = PercentileFromSorted(batch, 90);
        const uint64_t p95 = PercentileFromSorted(batch, 95);
        const uint64_t p99 = PercentileFromSorted(batch, 99);

        std::error_code ec;
        std::uintmax_t file_size = std::filesystem::file_size(kLatencyCsvPath, ec);
        if (!ec && file_size == 0) {
            latency_csv << "utc_s,samples,p50_ns,p90_ns,p95_ns,p99_ns" << std::endl;
        }

        latency_csv << NowSeconds() << ',' << kLatencyBatchSize << ',' << p50 << ',' << p90 << ',' << p95 << ',' << p99 << std::endl;

        batch.clear();
    }
}

void ProcessLatencyAggregation() {
    std::ofstream latency_csv;
    latency_csv.open(kLatencyCsvPath, std::ios::app);
    if (!latency_csv.is_open()) {
        LOG_ERROR << "Cannot open latency CSV " << kLatencyCsvPath << Endl;
        std::exit(1);
    }

    while (true) {
        if (ProcessLatencyAggregationAttempt(latency_csv) != 0) {
            latency_csv.close();
            std::exit(1);
        }
        std::this_thread::sleep_for(std::chrono::seconds(kReconnectTimeoutSeconds));
        LOG_WARNING << "Restarting latency aggregation" << Endl;
    }
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

    std::vector<std::pair<std::string, std::string>> log_routes;
    log_routes.emplace_back(IpcFeederToObserverShmName(), kLogfilesDir + "feeder.log");
    log_routes.emplace_back(IpcExecutorToObserverShmName(), kLogfilesDir + "executor.log");
    for (TraderId id : kTraderIds) {
        const TraderConfig& cfg = GetTraderConfig(id);
        log_routes.emplace_back(cfg.trader_observer_shm, kLogfilesDir + GetTraderLogfilePath(id));
    }

    std::vector<std::thread> threads;
    threads.reserve(log_routes.size() + 1);
    for (const auto& [shm_name, logfile_path] : log_routes) {
        threads.emplace_back(ProcessLog, shm_name.c_str(), logfile_path);
    }
    threads.emplace_back(ProcessLatencyAggregation);

    for (auto& thread : threads) {
        thread.join();
    }

    LOG_INFO << "Finishing Observer" << Endl;
}

}  // namespace hft

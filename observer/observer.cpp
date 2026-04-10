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

void WriteLatencyCsvHeader(std::ofstream& latency_csv) {
    latency_csv
        << "utc_s,samples,"
        << "total_p50_ns,total_p90_ns,total_p95_ns,total_p99_ns,"
        << "feeder_p50_ns,feeder_p90_ns,feeder_p95_ns,feeder_p99_ns,"
        << "md_queue_p50_ns,md_queue_p90_ns,md_queue_p95_ns,md_queue_p99_ns,"
        << "trader_p50_ns,trader_p90_ns,trader_p95_ns,trader_p99_ns,"
        << "order_queue_p50_ns,order_queue_p90_ns,order_queue_p95_ns,order_queue_p99_ns"
        << std::endl;
}

void SortAndWriteLatencyPercentiles(std::ofstream& latency_csv, std::vector<uint64_t>& values) {
    std::sort(values.begin(), values.end());
    latency_csv << ',' << PercentileFromSorted(values, 50)
                << ',' << PercentileFromSorted(values, 90)
                << ',' << PercentileFromSorted(values, 95)
                << ',' << PercentileFromSorted(values, 99);
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

    std::vector<uint64_t> batch_total;
    std::vector<uint64_t> batch_feeder_ns;
    std::vector<uint64_t> batch_md_queue_ns;
    std::vector<uint64_t> batch_trader_ns;
    std::vector<uint64_t> batch_order_queue_ns;
    batch_total.reserve(kLatencyBatchSize);
    batch_feeder_ns.reserve(kLatencyBatchSize);
    batch_md_queue_ns.reserve(kLatencyBatchSize);
    batch_trader_ns.reserve(kLatencyBatchSize);
    batch_order_queue_ns.reserve(kLatencyBatchSize);

    const auto clear_batches = [&]() {
        batch_total.clear();
        batch_feeder_ns.clear();
        batch_md_queue_ns.clear();
        batch_trader_ns.clear();
        batch_order_queue_ns.clear();
    };

    while (true) {
        while (batch_total.size() < kLatencyBatchSize) {
            LatencyNsSample sample;
            ReadResult result = ring_buffer->Read(sample);
            if (result == ReadResult::kSuccess) {
                batch_total.push_back(sample.delta_total);
                batch_feeder_ns.push_back(sample.delta_feeder_ns);
                batch_md_queue_ns.push_back(sample.delta_md_queue_ns);
                batch_trader_ns.push_back(sample.delta_trader_ns);
                batch_order_queue_ns.push_back(sample.delta_order_queue_ns);
            } else if (result == ReadResult::kConsumerIsDisabled) {
                LOG_WARNING << "Latency consumer is disabled; reset and drop partial batch" << Endl;
                clear_batches();
                ring_buffer->ResetConsumer();
            } else if (!shm->IsProducerAlive(kLivenessThresholdSeconds)) {
                LOG_WARNING << "Latency producer is dead" << Endl;
                return 0;
            }
        }

        std::error_code ec;
        std::uintmax_t file_size = std::filesystem::file_size(kLatencyCsvPath, ec);
        if (!ec && file_size == 0) {
            WriteLatencyCsvHeader(latency_csv);
        }

        latency_csv << NowSeconds() << ',' << kLatencyBatchSize;
        SortAndWriteLatencyPercentiles(latency_csv, batch_total);
        SortAndWriteLatencyPercentiles(latency_csv, batch_feeder_ns);
        SortAndWriteLatencyPercentiles(latency_csv, batch_md_queue_ns);
        SortAndWriteLatencyPercentiles(latency_csv, batch_trader_ns);
        SortAndWriteLatencyPercentiles(latency_csv, batch_order_queue_ns);
        latency_csv << std::endl;

        clear_batches();
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

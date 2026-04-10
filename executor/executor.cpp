#include "executor.hpp"

#include <lib/common.hpp>
#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_params.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace hft {

namespace {

constexpr uint32_t kReconnectTimeoutMs = 100;
constexpr uint32_t kLivenessThresholdSeconds = 5;

constexpr const char* SymbolLabel(Symbol symbol) noexcept {
    switch (symbol) {
        case Symbol::kBtcUsdt:
            return "BTCUSDT";
        case Symbol::kEthUsdt:
            return "ETHUSDT";
        default:
            return "";
    }
}

constexpr uint64_t PriceScaleDivisor() noexcept {
    uint64_t d = 1;
    for (uint32_t i = 0; i < kPriceShift; ++i) {
        d *= 10;
    }
    return d;
}

double PriceFromScaled(uint64_t scaled) noexcept {
    return static_cast<double>(scaled) / static_cast<double>(PriceScaleDivisor());
}

uint64_t MonotonicDeltaNs(uint64_t end_ns, uint64_t begin_ns) noexcept {
    if (begin_ns == 0 || end_ns < begin_ns) {
        return 0;
    }
    return end_ns - begin_ns;
}

}  // namespace

int RunExecutor() {
    std::unique_ptr<ShmToObserver> shm_log = nullptr;
    try {
        RemoveSharedMemory(IpcExecutorToObserverShmName());
        shm_log = std::make_unique<ShmToObserver>(IpcExecutorToObserverShmName(), MemoryRole::kCreateOnly);
        shm_log->UpdateHeartbeat();
    } catch (const std::exception&) {
        return 1;
    }

    auto [ring_buffer_log] = shm_log->GetObjects();
    HotPathLogger::Init(ring_buffer_log);
    HOT_INFO << "Executor started!" << Endl;

    std::unique_ptr<ShmLatency> shm_latency = nullptr;
    try {
        RemoveSharedMemory(IpcLatencyShmName());
        shm_latency = std::make_unique<ShmLatency>(IpcLatencyShmName(), MemoryRole::kCreateOnly);
        shm_latency->UpdateHeartbeat();
    } catch (const std::exception& e) {
        HOT_ERROR << "Failed to create latency shm: " << IpcLatencyShmName() << ": " << e.what() << Endl;
        return 1;
    }
    HOT_INFO << "Latency shm created: " << IpcLatencyShmName() << Endl;

    auto [ring_buffer_latency] = shm_latency->GetObjects();

    ArrayByTraderId<std::unique_ptr<ShmOrder>> shm_order{};
    ArrayByTraderId<OrderRingBuffer*> rb_order{};

    for (TraderId id : kTraderIds) {
        while (shm_order[id] == nullptr) {
            try {
                const TraderConfig& cfg = GetTraderConfig(id);
                shm_order[id] = std::make_unique<ShmOrder>(cfg.order_shm.c_str(), MemoryRole::kOpenOnly);
            } catch (const ShmVersionConflict& e) {
                HOT_ERROR << e.what() << Endl;
                return 1;
            } catch (const std::exception& e) {
                HOT_WARNING << "Failed to open order shm for trader " << std::to_underlying(id) << ": " << e.what() << Endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(kReconnectTimeoutMs));
            }
        }
        rb_order[id] = std::get<OrderRingBuffer*>(shm_order[id]->GetObjects());
        HOT_INFO << "Order shm opened for trader " << std::to_underlying(id) << Endl;
    }

    Order order;
    while (true) {
        shm_log->UpdateHeartbeat();
        shm_latency->UpdateHeartbeat();
        for (TraderId id : kTraderIds) {
            if (!shm_order[id]->IsProducerAlive(kLivenessThresholdSeconds)) {
                HOT_ERROR << "Producer " << std::to_underlying(id) << " is dead" << Endl;
                return 1;
            }
            const ReadResult result = rb_order[id]->Read(order);
            if (result == ReadResult::kSuccess) {
                const uint64_t now_steady_ns = SteadyNanoseconds();
                LatencyNsSample sample = {
                    .delta_total = MonotonicDeltaNs(now_steady_ns, order.feeder_read_steady_ns),
                    .delta_feeder_ns = MonotonicDeltaNs(order.feeder_write_steady_ns, order.feeder_read_steady_ns),
                    .delta_md_queue_ns = MonotonicDeltaNs(order.trader_read_steady_ns, order.feeder_write_steady_ns),
                    .delta_trader_ns = MonotonicDeltaNs(order.trader_write_steady_ns, order.trader_read_steady_ns),
                    .delta_order_queue_ns = MonotonicDeltaNs(now_steady_ns, order.trader_write_steady_ns),
                };
                ring_buffer_latency->Write(sample);
                HOT_INFO << SymbolLabel(order.symbol) << " "
                         << (order.type == Order::Type::kBuy ? "BUY" : "SELL")
                         << " price=" << PriceFromScaled(order.price) << " qty=" << order.quantity
                         << " latency_ns=" << sample.delta_total << Endl;
            } else if (result == ReadResult::kConsumerIsDisabled) {
                HOT_ERROR << "Consumer " << std::to_underlying(id) << " is disabled" << Endl;
                return 1;
            }
        }
    }

    HOT_INFO << "Finishing Executor" << Endl;
    return 0;
}

}  // namespace hft

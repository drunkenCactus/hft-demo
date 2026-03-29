#include "executor.hpp"

#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>
#include <lib/interprocess/ipc_params.hpp>

#include <array>
#include <chrono>
#include <cstddef>
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

    std::array<std::unique_ptr<ShmOrder>, kTraderCount> shm_order{};
    std::array<OrderRingBuffer*, kTraderCount> rb_order{};

    for (std::size_t i = 0; i < kTraderCount; ++i) {
        while (shm_order[i] == nullptr) {
            try {
                const TraderConfig& cfg = GetTraderConfig(static_cast<TraderId>(i));
                shm_order[i] = std::make_unique<ShmOrder>(cfg.order_shm.c_str(), MemoryRole::kOpenOnly);
            } catch (const ShmVersionConflict& e) {
                HOT_ERROR << e.what() << Endl;
                return 1;
            } catch (const std::exception& e) {
                HOT_WARNING << "Failed to open order shm for trader " << i << ": " << e.what() << Endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(kReconnectTimeoutMs));
            }
        }
        rb_order[i] = std::get<OrderRingBuffer*>(shm_order[i]->GetObjects());
        HOT_INFO << "Order shm opened for trader " << i << Endl;
    }

    Order order;
    while (true) {
        shm_log->UpdateHeartbeat();
        for (std::size_t i = 0; i < kTraderCount; ++i) {
            if (!shm_order[i]->IsProducerAlive(kLivenessThresholdSeconds)) {
                HOT_ERROR << "Producer " << i << " is dead" << Endl;
                return 1;
            }
            const ReadResult result = rb_order[i]->Read(order);
            if (result == ReadResult::kSuccess) {
                HOT_INFO << SymbolLabel(order.symbol) << " "
                         << (order.type == Order::Type::kBuy ? "BUY" : "SELL") << " price=" << order.price
                         << " qty=" << order.quantity << Endl;
            } else if (result == ReadResult::kConsumerIsDisabled) {
                HOT_ERROR << "Consumer " << i << " is disabled" << Endl;
                return 1;
            }
        }
    }

    HOT_INFO << "Finishing Executor" << Endl;
    return 0;
}

}  // namespace hft

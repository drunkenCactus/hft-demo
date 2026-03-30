#include "executor.hpp"

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
        for (TraderId id : kTraderIds) {
            if (!shm_order[id]->IsProducerAlive(kLivenessThresholdSeconds)) {
                HOT_ERROR << "Producer " << std::to_underlying(id) << " is dead" << Endl;
                return 1;
            }
            const ReadResult result = rb_order[id]->Read(order);
            if (result == ReadResult::kSuccess) {
                HOT_INFO << SymbolLabel(order.symbol) << " "
                         << (order.type == Order::Type::kBuy ? "BUY" : "SELL") << " price=" << order.price
                         << " qty=" << order.quantity << Endl;
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

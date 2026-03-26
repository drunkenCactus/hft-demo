#pragma once

#include <array>
#include <cstdint>

namespace hft {

class TradeFlowWindow {
public:
    static constexpr uint64_t kDefaultWindowUs = 250'000;

    explicit TradeFlowWindow(uint64_t window_us = kDefaultWindowUs) noexcept;

    void OnTrade(uint64_t event_ts_us, bool is_buyer_maker, uint64_t quantity) noexcept;

    uint64_t AggressiveBuyVolume() const noexcept;

    uint64_t AggressiveSellVolume() const noexcept;

private:
    void EvictOlderThan(uint64_t min_ts_us) noexcept;

    void PopOldest() noexcept;

private:
    struct Entry {
        uint64_t event_ts_us = 0;
        uint64_t aggressive_buy_qty = 0;
        uint64_t aggressive_sell_qty = 0;
    };

    static constexpr uint32_t kCapacity = 2048;

    std::array<Entry, kCapacity> ring_{};
    uint32_t head_ = 0;
    uint32_t tail_ = 0;
    uint32_t size_ = 0;
    uint64_t sum_aggressive_buy_ = 0;
    uint64_t sum_aggressive_sell_ = 0;
    uint64_t window_us_;
};

}  // namespace hft

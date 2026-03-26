#include <lib/trade_flow_window.hpp>

namespace hft {

TradeFlowWindow::TradeFlowWindow(uint64_t window_us) noexcept
    : window_us_(window_us)
{}

void TradeFlowWindow::OnTrade(uint64_t event_ts_us, bool is_buyer_maker, uint64_t quantity) noexcept {
    const uint64_t cutoff = event_ts_us > window_us_ ? event_ts_us - window_us_ : 0;
    EvictOlderThan(cutoff);

    const uint64_t buy_qty = is_buyer_maker ? 0 : quantity;
    const uint64_t sell_qty = is_buyer_maker ? quantity : 0;

    if (size_ == kCapacity) {
        PopOldest();
    }
    ring_[tail_] = {event_ts_us, buy_qty, sell_qty};
    tail_ = (tail_ + 1) % kCapacity;
    ++size_;
    sum_aggressive_buy_ += buy_qty;
    sum_aggressive_sell_ += sell_qty;
}

uint64_t TradeFlowWindow::AggressiveBuyVolume() const noexcept {
    return sum_aggressive_buy_;
}

uint64_t TradeFlowWindow::AggressiveSellVolume() const noexcept {
    return sum_aggressive_sell_;
}

void TradeFlowWindow::EvictOlderThan(uint64_t min_ts_us) noexcept {
    while (size_ > 0 && ring_[head_].event_ts_us < min_ts_us) {
        PopOldest();
    }
}

void TradeFlowWindow::PopOldest() noexcept {
    sum_aggressive_buy_ -= ring_[head_].aggressive_buy_qty;
    sum_aggressive_sell_ -= ring_[head_].aggressive_sell_qty;
    head_ = (head_ + 1) % kCapacity;
    --size_;
}

}  // namespace hft

#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace hft {

struct OrderBookRow {
    uint64_t price = 0;
    uint64_t quantity = 0;
};

enum class OrderBookSideOrder {
    kPriceAscending,
    kPriceDescending,
};

template <uint32_t Depth, OrderBookSideOrder Order>
class OrderBookSide {
public:
    void Init(const uint64_t* prices, const uint64_t* quantities, uint32_t count) noexcept {
        count_ = count;
        if (count_ > Depth) {
            count_ = Depth;
        }
        for (uint32_t i = 0; i < count_; ++i) {
            rows_[i].price = prices[i];
            rows_[i].quantity = quantities[i];
        }
    }

    void Update(uint64_t price, uint64_t quantity) noexcept {
        OrderBookRow* const end = rows_ + count_;
        OrderBookRow* it;
        if constexpr (Order == OrderBookSideOrder::kPriceAscending) {
            it = std::lower_bound(
                rows_,
                end,
                price,
                [](const OrderBookRow& row, uint64_t key) {
                    return row.price < key;
                }
            );
        } else {
            it = std::lower_bound(
                rows_,
                end,
                price,
                [](const OrderBookRow& row, uint64_t key) {
                    return row.price > key;
                }
            );
        }
        const uint32_t pos = static_cast<uint32_t>(it - rows_);
        if (quantity == 0U) {
            if (it != end && it->price == price) {
                RemoveAt(pos);
            }
            return;
        }
        if (it != end && it->price == price) {
            rows_[pos].quantity = quantity;
            return;
        }
        InsertAt(pos, price, quantity);
    }

    const OrderBookRow& GetBest() const noexcept {
        constexpr static OrderBookRow empty;
        if (count_ == 0) {
            return empty;
        }
        return rows_[0];
    }

    std::span<const OrderBookRow> Get() const noexcept {
        return std::span<const OrderBookRow>(rows_, count_);
    }

private:
    void RemoveAt(uint32_t index) noexcept {
        std::memmove(
            rows_ + index,
            rows_ + index + 1,
            (count_ - index - 1) * sizeof(OrderBookRow)
        );
        --count_;
    }

    void InsertAt(uint32_t pos, uint64_t price, uint64_t quantity) noexcept {
        if (pos > count_) {
            return;
        }
        if (count_ < Depth) {
            std::memmove(
                rows_ + pos + 1,
                rows_ + pos,
                (count_ - pos) * sizeof(OrderBookRow)
            );
            rows_[pos].price = price;
            rows_[pos].quantity = quantity;
            ++count_;
            return;
        }
        if (pos >= Depth) {
            return;
        }
        std::memmove(
            rows_ + pos + 1,
            rows_ + pos,
            (Depth - 1 - pos) * sizeof(OrderBookRow)
        );
        rows_[pos].price = price;
        rows_[pos].quantity = quantity;
    }

    OrderBookRow rows_[Depth]{};
    uint32_t count_ = 0;
};

template <uint32_t Depth>
class OrderBook_ {
public:
    void Init(
        uint64_t last_update_id,
        const uint64_t* bids_prices,
        const uint64_t* bids_quantities,
        uint32_t bids_depth,
        const uint64_t* asks_prices,
        const uint64_t* asks_quantities,
        uint32_t asks_depth
    ) noexcept {
        last_update_id_ = last_update_id;
        bids_.Init(bids_prices, bids_quantities, bids_depth);
        asks_.Init(asks_prices, asks_quantities, asks_depth);
    }

    void UpdateBid(uint64_t last_update_id, uint64_t price, uint64_t quantity) noexcept {
        last_update_id_ = last_update_id;
        bids_.Update(price, quantity);
    }

    void UpdateAsk(uint64_t last_update_id, uint64_t price, uint64_t quantity) noexcept {
        last_update_id_ = last_update_id;
        asks_.Update(price, quantity);
    }

    uint64_t LastUpdateId() const noexcept {
        return last_update_id_;
    }

    const OrderBookRow& GetBestBid() const noexcept {
        return bids_.GetBest();
    }

    const OrderBookRow& GetBestAsk() const noexcept {
        return asks_.GetBest();
    }

    std::span<const OrderBookRow> GetBids() const noexcept {
        return bids_.Get();
    }

    std::span<const OrderBookRow> GetAsks() const noexcept {
        return asks_.Get();
    }

    std::span<const OrderBookRow> GetTopBids(uint32_t n) const noexcept {
        const std::span<const OrderBookRow> all = bids_.Get();
        if (n == 0 || all.empty()) {
            return {};
        }
        const size_t len = std::min(static_cast<size_t>(n), all.size());
        return all.subspan(0, len);
    }

    std::span<const OrderBookRow> GetTopAsks(uint32_t n) const noexcept {
        const std::span<const OrderBookRow> all = asks_.Get();
        if (n == 0 || all.empty()) {
            return {};
        }
        const size_t len = std::min(static_cast<size_t>(n), all.size());
        return all.subspan(0, len);
    }

private:
    OrderBookSide<Depth, OrderBookSideOrder::kPriceDescending> bids_;
    OrderBookSide<Depth, OrderBookSideOrder::kPriceAscending> asks_;
    uint64_t last_update_id_ = 0;

    static_assert(Depth > 0, "Depth must be greater than zero");
};

}  // namespace hft

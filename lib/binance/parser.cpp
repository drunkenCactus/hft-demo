#include "parser.hpp"

#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace hft {

namespace {

Symbol SymbolFromString(const char* s, std::size_t len) noexcept {
    if (!s) {
        return Symbol::kUnknown;
    }
    if (len == 7 && std::strncmp(s, "BTCUSDT", 7) == 0) {
        return Symbol::kBtcUsdt;
    }
    if (len == 7 && std::strncmp(s, "ETHUSDT", 7) == 0) {
        return Symbol::kEthUsdt;
    }
    return Symbol::kUnknown;
}

uint64_t GetUint64FromMixed(int64_t i, uint64_t u, bool is_int) noexcept {
    if (is_int) {
        return static_cast<uint64_t>(i);
    }
    return u;
}

constexpr uint64_t kEventTimeMicrosecondThreshold = 1'000'000'000'000'000ULL;

inline uint64_t EventTimeToMicroseconds(uint64_t raw) noexcept {
    return (raw >= kEventTimeMicrosecondThreshold) ? raw : raw * 1000;
}

// 10^0 .. 10^18 (index k => 10^k); enough for fixed decimal and kPriceShift/kQuantityShift.
constexpr uint64_t kPow10U64[19] = {
    1ULL,
    10ULL,
    100ULL,
    1'000ULL,
    10'000ULL,
    100'000ULL,
    1'000'000ULL,
    10'000'000ULL,
    100'000'000ULL,
    1'000'000'000ULL,
    10'000'000'000ULL,
    100'000'000'000ULL,
    1'000'000'000'000ULL,
    10'000'000'000'000ULL,
    100'000'000'000'000ULL,
    1'000'000'000'000'000ULL,
    10'000'000'000'000'000ULL,
    100'000'000'000'000'000ULL,
    1'000'000'000'000'000'000ULL,
};

static_assert(kPriceShift <= 18U && kQuantityShift <= 18U);

constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();

// Assembles int_part * 10^dp + frac_part * 10^fp into uint64_t; false if result would exceed UINT64_MAX.
bool AssembleFixedDecimalToUint64(
    uint64_t int_part,
    uint32_t decimal_places,
    uint64_t frac_part,
    uint32_t frac_pad,
    uint64_t& out
) noexcept {
    const uint64_t scale_int = kPow10U64[decimal_places];
    const uint64_t scale_frac = kPow10U64[frac_pad];

    uint64_t term1 = 0;
    if (int_part != 0U) {
        if (int_part > kUint64Max / scale_int) {
            return false;
        }
        term1 = int_part * scale_int;
    }

    uint64_t term2 = 0;
    if (frac_part != 0U) {
        if (frac_part > kUint64Max / scale_frac) {
            return false;
        }
        term2 = frac_part * scale_frac;
    }

    if (term1 > kUint64Max - term2) {
        return false;
    }
    out = term1 + term2;
    return true;
}

// Parses decimal string as fixed-point uint64_t (scale 10^decimal_places). No floating-point.
// Allowed characters are digits and at most one '.'; any other character yields false.
// Single pass over the string; `decimal_places` must be <= 18 (see kPow10U64).
bool ParseFixedDecimalString(const char* first, const char* last, uint32_t decimal_places, uint64_t& out) noexcept {
    if (first >= last || decimal_places > 18U) {
        return false;
    }

    const char* p = first;
    uint64_t int_part = 0;
    uint64_t frac_part = 0;
    uint32_t frac_digits = 0;
    bool has_digit = false;
    bool saw_dot = false;

    while (p < last) {
        const char c = *p;
        if (c >= '0' && c <= '9') {
            const int digit = c - '0';
            has_digit = true;
            if (!saw_dot) {
                if (int_part > (std::numeric_limits<uint64_t>::max() - static_cast<uint64_t>(digit)) / 10ULL) {
                    return false;
                }
                int_part = int_part * 10ULL + static_cast<uint64_t>(digit);
            } else {
                if (frac_digits < decimal_places) {
                    if (frac_part > (std::numeric_limits<uint64_t>::max() - static_cast<uint64_t>(digit)) / 10ULL) {
                        return false;
                    }
                    frac_part = frac_part * 10ULL + static_cast<uint64_t>(digit);
                    ++frac_digits;
                }
            }
            ++p;
            continue;
        }
        if (c == '.') {
            if (saw_dot) {
                return false;
            }
            saw_dot = true;
            ++p;
            continue;
        }
        return false;
    }

    if (!has_digit) {
        return false;
    }

    const uint32_t frac_pad = decimal_places - frac_digits;
    return AssembleFixedDecimalToUint64(int_part, decimal_places, frac_part, frac_pad, out);
}

bool IsDepthStream(std::string_view stream) noexcept {
    return stream.size() >= 6 && (stream.ends_with("@depth@100ms") || stream.ends_with("@depth"));
}

bool IsTradeStream(std::string_view stream) noexcept {
    return stream.size() >= 6 && stream.ends_with("@trade");
}

bool JsonRootIsObject(std::string_view json) noexcept {
    const char* p = json.data();
    const char* const end = p + json.size();
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p < end && *p == '{';
}

// --- depth update (SAX) ---

// according to binance documentation, the maximum depth is 5000
constexpr std::size_t kMaxDepthLevelsPerSide = 5000;

enum class DepthPending : std::uint8_t {
    kNone,
    kEventType,
    kEventTime,
    kSymbol,
    kFirstUpdateId,
    kLastUpdateId,
    kBids,
    kAsks,
};

class DepthSaxHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>> {
public:
    explicit DepthSaxHandler(std::function<void(OrderBookUpdate&)> callback) noexcept
        : callback_(std::move(callback))
    {}

    bool Default() {
        return false;
    }

    bool Null() {
        return false;
    }

    bool Bool(bool /*b*/) {
        return false;
    }

    bool Int(int i) {
        return Int64(static_cast<int64_t>(i));
    }

    bool Uint(unsigned i) {
        return Uint64(static_cast<uint64_t>(i));
    }

    bool Int64(int64_t i) {
        return Number(i, static_cast<uint64_t>(i), true);
    }

    bool Uint64(uint64_t i) {
        return Number(static_cast<int64_t>(i), i, false);
    }

    bool Double(double /*d*/) {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        if (array_depth_ == 2) {
            if (stop_side_levels_) {
                return true;
            }
            if (row_string_idx_ == 0) {
                constexpr std::size_t kRow0Cap = sizeof(row0_storage_) - 1;
                if (static_cast<std::size_t>(len) > kRow0Cap) {
                    stop_side_levels_ = true;
                    return true;
                }
                std::memcpy(row0_storage_, str, len);
                row0_storage_[len] = '\0';
                row0_len_ = len;
                row_string_idx_ = 1;
                return true;
            }
            if (row_string_idx_ == 1) {
                uint64_t price = 0;
                uint64_t qty = 0;
                const char* const p0 = row0_storage_;
                const char* const p0e = p0 + row0_len_;
                if (!ParseFixedDecimalString(p0, p0e, kPriceShift, price) ||
                    !ParseFixedDecimalString(str, str + len, kQuantityShift, qty)
                ) {
                    stop_side_levels_ = true;
                    row_string_idx_ = 0;
                    return true;
                }
                if (current_side_is_bid_) {
                    if (bid_n_ >= kMaxDepthLevelsPerSide) {
                        return false;
                    }
                    bid_prices_[bid_n_] = price;
                    bid_qtys_[bid_n_] = qty;
                    ++bid_n_;
                } else {
                    if (ask_n_ >= kMaxDepthLevelsPerSide) {
                        return false;
                    }
                    ask_prices_[ask_n_] = price;
                    ask_qtys_[ask_n_] = qty;
                    ++ask_n_;
                }
                row_string_idx_ = 0;
                return true;
            }
            return false;
        }

        switch (pending_) {
        case DepthPending::kEventType:
            pending_ = DepthPending::kNone;
            if (len == 11 && std::memcmp(str, "depthUpdate", 11) == 0) {
                valid_event_type_ = true;
                return true;
            }
            return false;
        case DepthPending::kSymbol:
            pending_ = DepthPending::kNone;
            symbol_ = SymbolFromString(str, len);
            return true;
        default:
            pending_ = DepthPending::kNone;
            return true;
        }
    }

    bool StartObject() {
        if (object_depth_ > 0) {
            return false;
        }
        ++object_depth_;
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        if (object_depth_ != 1 || array_depth_ != 0) {
            return false;
        }
        if (len == 1) {
            switch (str[0]) {
            case 'e':
                pending_ = DepthPending::kEventType;
                return true;
            case 'E':
                pending_ = DepthPending::kEventTime;
                return true;
            case 's':
                pending_ = DepthPending::kSymbol;
                return true;
            case 'U':
                pending_ = DepthPending::kFirstUpdateId;
                return true;
            case 'u':
                pending_ = DepthPending::kLastUpdateId;
                return true;
            case 'b':
                pending_ = DepthPending::kBids;
                return true;
            case 'a':
                pending_ = DepthPending::kAsks;
                return true;
            default:
                pending_ = DepthPending::kNone;
                return true;
            }
        }
        pending_ = DepthPending::kNone;
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/) {
        if (object_depth_ != 1) {
            return false;
        }
        --object_depth_;
        if (!valid_event_type_) {
            return false;
        }
        FlushLevels();
        return true;
    }

    bool StartArray() {
        if (array_depth_ == 0) {
            if (pending_ == DepthPending::kBids) {
                pending_ = DepthPending::kNone;
                current_side_is_bid_ = true;
                stop_side_levels_ = false;
                ++array_depth_;
                return true;
            }
            if (pending_ == DepthPending::kAsks) {
                pending_ = DepthPending::kNone;
                current_side_is_bid_ = false;
                stop_side_levels_ = false;
                ++array_depth_;
                return true;
            }
            return false;
        }
        if (array_depth_ == 1) {
            ++array_depth_;
            row_string_idx_ = 0;
            return true;
        }
        return false;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/) {
        if (array_depth_ == 2) {
            if (row_string_idx_ != 0) {
                stop_side_levels_ = true;
            }
            row_string_idx_ = 0;
            --array_depth_;
            return true;
        }
        if (array_depth_ == 1) {
            --array_depth_;
            return true;
        }
        return false;
    }

private:
    bool Number(int64_t i, uint64_t u, bool is_int) {
        const uint64_t v = GetUint64FromMixed(i, u, is_int);
        switch (pending_) {
        case DepthPending::kEventTime:
            pending_ = DepthPending::kNone;
            event_ts_ = EventTimeToMicroseconds(v);
            return true;
        case DepthPending::kFirstUpdateId:
            pending_ = DepthPending::kNone;
            first_id_ = v;
            return true;
        case DepthPending::kLastUpdateId:
            pending_ = DepthPending::kNone;
            last_id_ = v;
            return true;
        default:
            pending_ = DepthPending::kNone;
            return true;
        }
    }

    void FlushLevels() {
        if (!callback_) {
            return;
        }
        const bool has_asks = ask_n_ > 0;
        OrderBookUpdate out{};
        out.event_timestamp_microseconds = event_ts_;
        out.first_update_id = first_id_;
        out.last_update_id = last_id_;
        out.symbol = symbol_;

        for (std::size_t i = 0; i < bid_n_; ++i) {
            out.type = OrderBookUpdate::Type::kBid;
            out.price = bid_prices_[i];
            out.quantity = bid_qtys_[i];
            out.has_more = has_asks || (i < bid_n_ - 1);
            callback_(out);
        }
        for (std::size_t i = 0; i < ask_n_; ++i) {
            out.type = OrderBookUpdate::Type::kAsk;
            out.price = ask_prices_[i];
            out.quantity = ask_qtys_[i];
            out.has_more = i < ask_n_ - 1;
            callback_(out);
        }
    }

private:
    std::function<void(OrderBookUpdate&)> callback_;

    int object_depth_ = 0;
    int array_depth_ = 0;
    DepthPending pending_ = DepthPending::kNone;
    bool valid_event_type_ = false;

    uint64_t event_ts_ = 0;
    uint64_t first_id_ = 0;
    uint64_t last_id_ = 0;
    Symbol symbol_ = Symbol::kUnknown;

    bool current_side_is_bid_ = false;
    bool stop_side_levels_ = false;
    int row_string_idx_ = 0;
    char row0_storage_[96];
    rapidjson::SizeType row0_len_ = 0;

    std::size_t bid_n_ = 0;
    std::size_t ask_n_ = 0;
    uint64_t bid_prices_[kMaxDepthLevelsPerSide];
    uint64_t bid_qtys_[kMaxDepthLevelsPerSide];
    uint64_t ask_prices_[kMaxDepthLevelsPerSide];
    uint64_t ask_qtys_[kMaxDepthLevelsPerSide];
};

// --- trade (SAX) ---

enum class TradePending : std::uint8_t {
    kNone,
    kEventType,
    kEventTime,
    kSymbol,
    kPrice,
    kQuantity,
    kBuyerMaker,
};

class TradeSaxHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>> {
public:
    explicit TradeSaxHandler(std::function<void(Trade&)> callback) noexcept
        : callback_(std::move(callback))
    {}

    bool Default() {
        return false;
    }

    bool Null() {
        return false;
    }

    bool Bool(bool b) {
        if (pending_ != TradePending::kBuyerMaker) {
            pending_ = TradePending::kNone;
            return true;
        }
        pending_ = TradePending::kNone;
        out_.is_buyer_maker = b;
        have_buyer_maker_ = true;
        return true;
    }

    bool Int(int i) {
        return Int64(static_cast<int64_t>(i));
    }

    bool Uint(unsigned i) {
        return Uint64(static_cast<uint64_t>(i));
    }

    bool Int64(int64_t i) {
        return Number(i, static_cast<uint64_t>(i), true);
    }

    bool Uint64(uint64_t i) {
        return Number(static_cast<int64_t>(i), i, false);
    }

    bool Double(double /*d*/) {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        switch (pending_) {
        case TradePending::kEventType:
            pending_ = TradePending::kNone;
            if (len == 5 && std::memcmp(str, "trade", 5) == 0) {
                valid_event_type_ = true;
                return true;
            }
            return false;
        case TradePending::kSymbol:
            pending_ = TradePending::kNone;
            out_.symbol = SymbolFromString(str, len);
            return true;
        case TradePending::kPrice:
            pending_ = TradePending::kNone;
            if (!ParseFixedDecimalString(str, str + len, kPriceShift, out_.price)) {
                return false;
            }
            have_price_ = true;
            return true;
        case TradePending::kQuantity:
            pending_ = TradePending::kNone;
            if (!ParseFixedDecimalString(str, str + len, kQuantityShift, out_.quantity)) {
                return false;
            }
            have_qty_ = true;
            return true;
        default:
            pending_ = TradePending::kNone;
            return true;
        }
    }

    bool StartObject() {
        if (object_depth_ > 0) {
            return false;
        }
        ++object_depth_;
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        if (object_depth_ != 1) {
            return false;
        }
        if (len == 1) {
            switch (str[0]) {
            case 'e':
                pending_ = TradePending::kEventType;
                return true;
            case 'E':
                pending_ = TradePending::kEventTime;
                return true;
            case 's':
                pending_ = TradePending::kSymbol;
                return true;
            case 'p':
                pending_ = TradePending::kPrice;
                return true;
            case 'q':
                pending_ = TradePending::kQuantity;
                return true;
            case 'm':
                pending_ = TradePending::kBuyerMaker;
                return true;
            default:
                pending_ = TradePending::kNone;
                return true;
            }
        }
        pending_ = TradePending::kNone;
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/) {
        if (object_depth_ != 1) {
            return false;
        }
        --object_depth_;
        if (!valid_event_type_ || !have_price_ || !have_qty_ || !have_buyer_maker_) {
            return false;
        }
        if (callback_) {
            callback_(out_);
        }
        return true;
    }

private:
    bool Number(int64_t i, uint64_t u, bool is_int) {
        const uint64_t v = GetUint64FromMixed(i, u, is_int);
        if (pending_ == TradePending::kEventTime) {
            pending_ = TradePending::kNone;
            out_.event_timestamp_microseconds = EventTimeToMicroseconds(v);
            return true;
        }
        pending_ = TradePending::kNone;
        return true;
    }

private:
    std::function<void(Trade&)> callback_;
    int object_depth_ = 0;
    TradePending pending_ = TradePending::kNone;
    bool valid_event_type_ = false;
    bool have_price_ = false;
    bool have_qty_ = false;
    bool have_buyer_maker_ = false;
    Trade out_{};
};

// --- order book snapshot (SAX) ---

class SnapshotSaxHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>> {
public:
    explicit SnapshotSaxHandler(std::function<void(const OrderBookSnapshot&)> callback) noexcept
        : callback_(std::move(callback))
    {}

    bool Default() {
        return false;
    }

    bool Null() {
        return false;
    }

    bool Bool(bool /*b*/) {
        return false;
    }

    bool Int(int i) {
        return Int64(static_cast<int64_t>(i));
    }

    bool Uint(unsigned i) {
        return Uint64(static_cast<uint64_t>(i));
    }

    bool Int64(int64_t i) {
        return Number(i, static_cast<uint64_t>(i), true);
    }

    bool Uint64(uint64_t i) {
        return Number(static_cast<int64_t>(i), i, false);
    }

    bool Double(double /*d*/) {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        if (array_depth_ == 2) {
            if (in_bids_ && bids_side_done_) {
                return true;
            }
            if (in_asks_ && asks_side_done_) {
                return true;
            }
            if (in_bids_) {
                if (out_.bids_depth >= kOrderBookDepth) {
                    return true;
                }
            } else if (in_asks_) {
                if (out_.asks_depth >= kOrderBookDepth) {
                    return true;
                }
            } else {
                return false;
            }
            if (row_string_idx_ == 0) {
                constexpr std::size_t kRow0Cap = sizeof(row0_storage_) - 1;
                if (static_cast<std::size_t>(len) > kRow0Cap) {
                    if (in_bids_) {
                        bids_side_done_ = true;
                    } else {
                        asks_side_done_ = true;
                    }
                    row_string_idx_ = 0;
                    return true;
                }
                std::memcpy(row0_storage_, str, len);
                row0_storage_[len] = '\0';
                row0_len_ = len;
                row_string_idx_ = 1;
                return true;
            }
            if (row_string_idx_ == 1) {
                uint64_t price = 0;
                uint64_t qty = 0;
                const char* const p0 = row0_storage_;
                const char* const p0e = p0 + row0_len_;
                if (!ParseFixedDecimalString(p0, p0e, kPriceShift, price) ||
                    !ParseFixedDecimalString(str, str + len, kQuantityShift, qty)
                ) {
                    if (in_bids_) {
                        bids_side_done_ = true;
                    } else {
                        asks_side_done_ = true;
                    }
                    row_string_idx_ = 0;
                    return true;
                }
                if (in_bids_) {
                    const uint32_t i = out_.bids_depth;
                    out_.bids_prices[i] = price;
                    out_.bids_quantities[i] = qty;
                    ++out_.bids_depth;
                } else {
                    const uint32_t i = out_.asks_depth;
                    out_.asks_prices[i] = price;
                    out_.asks_quantities[i] = qty;
                    ++out_.asks_depth;
                }
                row_string_idx_ = 0;
                return true;
            }
            return false;
        }
        pending_snapshot_ = 0;
        return true;
    }

    bool StartObject() {
        if (object_depth_ > 0) {
            return false;
        }
        ++object_depth_;
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType len, bool /*copy*/) {
        if (object_depth_ != 1 || array_depth_ != 0) {
            return false;
        }
        if (len == 12 && std::memcmp(str, "lastUpdateId", 12) == 0) {
            pending_snapshot_ = 1;
            return true;
        }
        if (len == 4 && std::memcmp(str, "bids", 4) == 0) {
            pending_snapshot_ = 2;
            return true;
        }
        if (len == 4 && std::memcmp(str, "asks", 4) == 0) {
            pending_snapshot_ = 3;
            return true;
        }
        pending_snapshot_ = 0;
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/) {
        if (object_depth_ != 1) {
            return false;
        }
        --object_depth_;
        if (array_depth_ != 0) {
            return false;
        }
        if (callback_) {
            callback_(out_);
        }
        return true;
    }

    bool StartArray() {
        if (object_depth_ != 1) {
            return false;
        }
        if (pending_snapshot_ == 2) {
            pending_snapshot_ = 0;
            in_bids_ = true;
            in_asks_ = false;
            ++array_depth_;
            return true;
        }
        if (pending_snapshot_ == 3) {
            pending_snapshot_ = 0;
            in_bids_ = false;
            in_asks_ = true;
            ++array_depth_;
            return true;
        }
        if (array_depth_ == 1) {
            ++array_depth_;
            row_string_idx_ = 0;
            return true;
        }
        return false;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/) {
        if (array_depth_ == 2) {
            row_string_idx_ = 0;
            --array_depth_;
            return true;
        }
        if (array_depth_ == 1) {
            --array_depth_;
            in_bids_ = false;
            in_asks_ = false;
            return true;
        }
        return false;
    }

private:
    bool Number(int64_t i, uint64_t u, bool is_int) {
        const uint64_t v = GetUint64FromMixed(i, u, is_int);
        if (pending_snapshot_ == 1) {
            pending_snapshot_ = 0;
            out_.last_update_id = v;
            return true;
        }
        pending_snapshot_ = 0;
        return true;
    }

private:
    std::function<void(const OrderBookSnapshot&)> callback_;
    int object_depth_ = 0;
    int array_depth_ = 0;
    int pending_snapshot_ = 0;
    bool in_bids_ = false;
    bool in_asks_ = false;
    bool bids_side_done_ = false;
    bool asks_side_done_ = false;
    int row_string_idx_ = 0;
    char row0_storage_[96];
    rapidjson::SizeType row0_len_ = 0;
    OrderBookSnapshot out_{};
};

// --- combined stream wrapper ---

class CombinedSaxHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>> {
public:
    CombinedSaxHandler(
        std::function<void(OrderBookUpdate&)> order_book_update_callback,
        std::function<void(Trade&)> trade_callback
    )
        : order_book_cb_(std::move(order_book_update_callback))
        , trade_cb_(std::move(trade_callback))
        , depth_(order_book_cb_)
        , trade_(trade_cb_)
    {}

    bool HaveStream() const noexcept {
        return have_stream_;
    }

    bool SawData() const noexcept {
        return saw_data_;
    }

    bool Default() {
        return false;
    }

    bool Null() {
        return false;
    }

    bool Bool(bool b) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            if (data_is_depth_) {
                return false;
            }
            return trade_.Bool(b);
        }
        return false;
    }

    bool Int(int i) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.Int(i) : trade_.Int(i);
        }
        return false;
    }

    bool Uint(unsigned i) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.Uint(i) : trade_.Uint(i);
        }
        return false;
    }

    bool Int64(int64_t i) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.Int64(i) : trade_.Int64(i);
        }
        return false;
    }

    bool Uint64(uint64_t i) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.Uint64(i) : trade_.Uint64(i);
        }
        return false;
    }

    bool Double(double /*d*/) {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType len, bool copy) {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.String(str, len, copy) : trade_.String(str, len, copy);
        }
        if (pending_stream_string_) {
            pending_stream_string_ = false;
            stream_len_ = std::min<std::size_t>(len, sizeof(stream_buf_) - 1);
            std::memcpy(stream_buf_, str, stream_len_);
            stream_buf_[stream_len_] = '\0';
            have_stream_ = true;
            return true;
        }
        return true;
    }

    bool StartObject() {
        if (expect_data_object_) {
            expect_data_object_ = false;
            if (!have_stream_) {
                return false;
            }
            const std::string_view stream(stream_buf_, stream_len_);
            if (!IsDepthStream(stream) && !IsTradeStream(stream)) {
                return false;
            }
            saw_data_ = true;
            in_data_ = true;
            data_is_depth_ = IsDepthStream(stream);
            if (data_is_depth_) {
                depth_ = DepthSaxHandler(order_book_cb_);
                return depth_.StartObject();
            }
            trade_ = TradeSaxHandler(trade_cb_);
            return trade_.StartObject();
        }
        if (in_data_) {
            return false;
        }
        ++root_depth_;
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType len, bool copy) {
        if (in_data_) {
            return data_is_depth_ ? depth_.Key(str, len, copy) : trade_.Key(str, len, copy);
        }
        if (len == 6 && std::memcmp(str, "stream", 6) == 0) {
            pending_stream_string_ = true;
            return true;
        }
        if (len == 4 && std::memcmp(str, "data", 4) == 0) {
            expect_data_object_ = true;
            return true;
        }
        return true;
    }

    bool EndObject(rapidjson::SizeType memberCount) {
        if (in_data_) {
            const bool ok = data_is_depth_ ? depth_.EndObject(memberCount) : trade_.EndObject(memberCount);
            if (!ok) {
                return false;
            }
            in_data_ = false;
            return true;
        }
        if (root_depth_ > 0) {
            --root_depth_;
        }
        return true;
    }

    bool StartArray() {
        if (expect_data_object_) {
            return false;
        }
        if (in_data_) {
            return data_is_depth_ ? depth_.StartArray() : trade_.StartArray();
        }
        return false;
    }

    bool EndArray(rapidjson::SizeType elementCount) {
        if (in_data_) {
            return data_is_depth_ ? depth_.EndArray(elementCount) : trade_.EndArray(elementCount);
        }
        return false;
    }

private:
    std::function<void(OrderBookUpdate&)> order_book_cb_;
    std::function<void(Trade&)> trade_cb_;

    int root_depth_ = 0;
    bool pending_stream_string_ = false;
    bool have_stream_ = false;
    char stream_buf_[256];
    std::size_t stream_len_ = 0;

    bool expect_data_object_ = false;
    bool saw_data_ = false;
    bool in_data_ = false;
    bool data_is_depth_ = false;

    DepthSaxHandler depth_;
    TradeSaxHandler trade_;
};

}  // namespace

bool ParseDepthEvent(
    std::string_view json,
    std::function<void(OrderBookUpdate&)> callback
) noexcept {
    DepthSaxHandler handler(std::move(callback));
    rapidjson::MemoryStream ms(json.data(), json.size());
    rapidjson::Reader reader;
    return reader.Parse(ms, handler);
}

bool ParseTradeEvent(
    std::string_view json,
    std::function<void(Trade&)> callback
) noexcept {
    TradeSaxHandler handler(std::move(callback));
    rapidjson::MemoryStream ms(json.data(), json.size());
    rapidjson::Reader reader;
    return reader.Parse(ms, handler);
}

bool ParseOrderBookSnapshot(
    std::string_view json,
    std::function<void(const OrderBookSnapshot&)> callback
) noexcept {
    if (!JsonRootIsObject(json)) {
        return false;
    }
    SnapshotSaxHandler handler(std::move(callback));
    rapidjson::MemoryStream ms(json.data(), json.size());
    rapidjson::Reader reader;
    return reader.Parse(ms, handler);
}

bool ParseEvent(
    std::string_view json,
    std::function<void(OrderBookUpdate&)> order_book_update_callback,
    std::function<void(Trade&)> trade_callback
) noexcept {
    CombinedSaxHandler handler(std::move(order_book_update_callback), std::move(trade_callback));
    rapidjson::MemoryStream ms(json.data(), json.size());
    rapidjson::Reader reader;
    if (!reader.Parse(ms, handler)) {
        return false;
    }
    return handler.HaveStream() && handler.SawData();
}

}  // namespace hft

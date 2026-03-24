#include "parser.hpp"

#include <lib/common.hpp>
#include <cstring>
#include <limits>
#include <rapidjson/document.h>

namespace hft {

namespace {

Symbol SymbolFromString(const char* s, std::size_t len) noexcept {
    if (!s) {
        return Symbol::UNKNOWN;
    }
    if (len == 7 && std::strncmp(s, "BTCUSDT", 7) == 0) {
        return Symbol::BTCUSDT;
    }
    if (len == 7 && std::strncmp(s, "ETHUSDT", 7) == 0) {
        return Symbol::ETHUSDT;
    }
    return Symbol::UNKNOWN;
}

uint64_t GetUint64(const rapidjson::Value& v) noexcept {
    if (v.IsUint64()) {
        return v.GetUint64();
    }
    if (v.IsInt64()) {
        return static_cast<uint64_t>(v.GetInt64());
    }
    return 0;
}

constexpr uint64_t EVENT_TIME_MICROSECOND_THRESHOLD = 1'000'000'000'000'000ULL;

inline uint64_t EventTimeToMicroseconds(uint64_t raw) noexcept {
    return (raw >= EVENT_TIME_MICROSECOND_THRESHOLD) ? raw : raw * 1000;
}

// 10^0 .. 10^18 (index k => 10^k); enough for fixed decimal and PRICE_SHIFT/QUANTITY_SHIFT.
constexpr uint64_t k_pow10_u64[19] = {
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

static_assert(PRICE_SHIFT <= 18U && QUANTITY_SHIFT <= 18U);

constexpr uint64_t k_uint64_max = std::numeric_limits<uint64_t>::max();

// Assembles int_part * 10^dp + frac_part * 10^fp into uint64_t; false if result would exceed UINT64_MAX.
[[nodiscard]] bool AssembleFixedDecimalToUint64(
    uint64_t int_part,
    uint32_t decimal_places,
    uint64_t frac_part,
    uint32_t frac_pad,
    uint64_t& out
) noexcept {
    const uint64_t scale_int = k_pow10_u64[decimal_places];
    const uint64_t scale_frac = k_pow10_u64[frac_pad];

    uint64_t term1 = 0;
    if (int_part != 0U) {
        if (int_part > k_uint64_max / scale_int) {
            return false;
        }
        term1 = int_part * scale_int;
    }

    uint64_t term2 = 0;
    if (frac_part != 0U) {
        if (frac_part > k_uint64_max / scale_frac) {
            return false;
        }
        term2 = frac_part * scale_frac;
    }

    if (term1 > k_uint64_max - term2) {
        return false;
    }
    out = term1 + term2;
    return true;
}

// Parses decimal string as fixed-point uint64_t (scale 10^decimal_places). No floating-point.
// Allowed characters are digits and at most one '.'; any other character yields false.
// Single pass over the string; `decimal_places` must be <= 18 (see k_pow10_u64).
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

bool ParsePriceQuantity(const rapidjson::Value& arr, uint64_t& price, uint64_t& quantity) noexcept {
    if (!arr.IsArray() || arr.Size() < 2) {
        return false;
    }
    const auto& p = arr[0];
    const auto& q = arr[1];
    if (!p.IsString() || !q.IsString()) {
        return false;
    }
    const char* price_start = p.GetString();
    const char* price_end = price_start + p.GetStringLength();
    const char* qty_start = q.GetString();
    const char* qty_end = qty_start + q.GetStringLength();
    if (!ParseFixedDecimalString(price_start, price_end, PRICE_SHIFT, price) ||
        !ParseFixedDecimalString(qty_start, qty_end, QUANTITY_SHIFT, quantity)) {
        return false;
    }
    return true;
}

bool ParseDepthEvent(const rapidjson::Value& value, std::function<void(const OrderBookUpdate&)> callback) noexcept {
    if (!value.IsObject()) {
        return false;
    }
    const auto e = value.FindMember("e");
    if (e == value.MemberEnd() || !e->value.IsString() ||
        std::strcmp(e->value.GetString(), "depthUpdate") != 0) {
        return false;
    }

    if (!callback) {
        return true;
    }

    const uint64_t parsing_ts = NowMicroseconds();

    const auto E = value.FindMember("E");
    uint64_t event_ts = 0;
    if (E != value.MemberEnd() && E->value.IsNumber()) {
        event_ts = EventTimeToMicroseconds(GetUint64(E->value));
    }

    const auto U = value.FindMember("U");
    const uint64_t first_id =
        (U != value.MemberEnd() && U->value.IsNumber()) ? GetUint64(U->value) : 0;

    const auto u = value.FindMember("u");
    const uint64_t last_id =
        (u != value.MemberEnd() && u->value.IsNumber()) ? GetUint64(u->value) : 0;

    Symbol symbol = Symbol::UNKNOWN;
    const auto s = value.FindMember("s");
    if (s != value.MemberEnd() && s->value.IsString()) {
        symbol = SymbolFromString(s->value.GetString(), s->value.GetStringLength());
    }

    OrderBookUpdate out{};
    out.meta.parsing_timestamp_microseconds = parsing_ts;
    out.meta.event_timestamp_microseconds = event_ts;
    out.first_update_id = first_id;
    out.last_update_id = last_id;
    out.symbol = symbol;

    const auto b = value.FindMember("b");
    if (b != value.MemberEnd() && b->value.IsArray()) {
        out.type = OrderBookUpdate::Type::BID;
        for (rapidjson::SizeType i = 0; i < b->value.Size(); ++i) {
            if (!ParsePriceQuantity(b->value[i], out.price, out.quantity)) {
                break;
            }
            callback(out);
        }
    }

    const auto a = value.FindMember("a");
    if (a != value.MemberEnd() && a->value.IsArray()) {
        out.type = OrderBookUpdate::Type::ASK;
        for (rapidjson::SizeType i = 0; i < a->value.Size(); ++i) {
            if (!ParsePriceQuantity(a->value[i], out.price, out.quantity)) {
                break;
            }
            callback(out);
        }
    }

    return true;
}

bool ParseTradeEvent(const rapidjson::Value& value, std::function<void(const Trade&)> callback) noexcept {
    if (!value.IsObject()) {
        return false;
    }
    const auto e = value.FindMember("e");
    if (e == value.MemberEnd() || !e->value.IsString() ||
        std::strcmp(e->value.GetString(), "trade") != 0) {
        return false;
    }

    Trade out{};
    out.meta.parsing_timestamp_microseconds = NowMicroseconds();

    const auto E = value.FindMember("E");
    if (E != value.MemberEnd() && E->value.IsNumber()) {
        out.meta.event_timestamp_microseconds = EventTimeToMicroseconds(GetUint64(E->value));
    }

    const auto s = value.FindMember("s");
    if (s != value.MemberEnd() && s->value.IsString()) {
        out.symbol = SymbolFromString(s->value.GetString(), s->value.GetStringLength());
    }

    const auto p = value.FindMember("p");
    if (p == value.MemberEnd() || !p->value.IsString()) {
        return false;
    }
    const char* price_start = p->value.GetString();
    const char* price_end = price_start + p->value.GetStringLength();
    if (!ParseFixedDecimalString(price_start, price_end, PRICE_SHIFT, out.price)) {
        return false;
    }

    const auto q = value.FindMember("q");
    if (q == value.MemberEnd() || !q->value.IsString()) {
        return false;
    }
    const char* qty_start = q->value.GetString();
    const char* qty_end = qty_start + q->value.GetStringLength();
    if (!ParseFixedDecimalString(qty_start, qty_end, QUANTITY_SHIFT, out.quantity)) {
        return false;
    }

    const auto buyer_maker = value.FindMember("m");
    if (buyer_maker == value.MemberEnd() || !buyer_maker->value.IsBool()) {
        return false;
    }
    out.is_buyer_maker = buyer_maker->value.GetBool();

    if (callback) {
        callback(out);
    }
    return true;
}

bool IsDepthStream(std::string_view stream) noexcept {
    return stream.size() >= 6 &&
           (stream.ends_with("@depth@100ms") || stream.ends_with("@depth"));
}

bool IsTradeStream(std::string_view stream) noexcept {
    return stream.size() >= 6 && stream.ends_with("@trade");
}

}  // namespace

bool ParseEvent(
    std::string_view json,
    std::function<void(const OrderBookUpdate&)> order_book_update_callback,
    std::function<void(const Trade&)> trade_callback
) noexcept {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }

    const auto stream_it = doc.FindMember("stream");
    if (stream_it == doc.MemberEnd() || !stream_it->value.IsString()) {
        return false;
    }
    const char* stream_str = stream_it->value.GetString();
    const std::size_t stream_len = stream_it->value.GetStringLength();
    const std::string_view stream(stream_str, stream_len);

    const auto data_it = doc.FindMember("data");
    if (data_it == doc.MemberEnd() || !data_it->value.IsObject()) {
        return false;
    }

    if (IsDepthStream(stream)) {
        return ParseDepthEvent(data_it->value, std::move(order_book_update_callback));
    }
    if (IsTradeStream(stream)) {
        return ParseTradeEvent(data_it->value, std::move(trade_callback));
    }
    return false;
}

bool ParseDepthEvent(std::string_view json, std::function<void(const OrderBookUpdate&)> callback) noexcept {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }
    return ParseDepthEvent(doc.GetObject(), callback);
}

bool ParseTradeEvent(std::string_view json, std::function<void(const Trade&)> callback) noexcept {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }
    return ParseTradeEvent(doc.GetObject(), callback);
}

bool ParseOrderBookSnapshot(std::string_view json, std::function<void(const OrderBookSnapshot&)> callback) noexcept {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }

    OrderBookSnapshot out{};
    out.meta.parsing_timestamp_microseconds = NowMicroseconds();

    const auto last_update_id_member = doc.FindMember("lastUpdateId");
    if (last_update_id_member != doc.MemberEnd() && last_update_id_member->value.IsNumber()) {
        out.last_update_id = GetUint64(last_update_id_member->value);
    }

    const auto bids = doc.FindMember("bids");
    if (bids != doc.MemberEnd() && bids->value.IsArray()) {
        const auto& arr = bids->value;
        const auto n = std::min(arr.Size(), static_cast<rapidjson::SizeType>(ORDER_BOOK_DEPTH));
        uint32_t count = 0;
        for (rapidjson::SizeType i = 0; i < n; ++i) {
            if (!ParsePriceQuantity(arr[i], out.bids_prices[i], out.bids_quantities[i])) {
                break;
            }
            ++count;
        }
        out.bids_depth = count;
    }

    const auto asks = doc.FindMember("asks");
    if (asks != doc.MemberEnd() && asks->value.IsArray()) {
        const auto& arr = asks->value;
        const auto n = std::min(arr.Size(), static_cast<rapidjson::SizeType>(ORDER_BOOK_DEPTH));
        uint32_t count = 0;
        for (rapidjson::SizeType i = 0; i < n; ++i) {
            if (!ParsePriceQuantity(arr[i], out.asks_prices[i], out.asks_quantities[i])) {
                break;
            }
            ++count;
        }
        out.asks_depth = count;
    }

    if (callback) {
        callback(out);
    }
    return true;
}

}  // namespace hft

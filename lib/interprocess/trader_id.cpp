#include <lib/interprocess/trader_id.hpp>

#include <cstdio>
#include <cstdlib>

namespace hft {

TraderId ParseTraderIdOrAbort(const std::string_view role) {
    if (role == "trader_btcusdt") {
        return TraderId::kBtcUsdt;
    }
    if (role == "trader_ethusdt") {
        return TraderId::kEthUsdt;
    }
    std::fprintf(
        stderr,
        "FATAL: unknown trader id (expected trader_btcusdt|trader_ethusdt), got \"%.*s\"\n",
        static_cast<int>(role.size()),
        role.data()
    );
    std::abort();
}

}  // namespace hft

#include "trader.hpp"

#include <lib/interprocess/trader_id.hpp>

#include <cstdio>
#include <cstring>
#include <sys/prctl.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "trader's instrument param is required\n");
        return 1;
    }
    char* const role = argv[1];
    const hft::TraderId trader_id = hft::ParseTraderIdOrAbort(role);
    // reset process name (instead of binary name)
    if (std::strlen(role) < 16) {
        (void)prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(static_cast<void*>(role)), 0UL, 0UL, 0UL);
    }
    return hft::RunTrader(trader_id);
}

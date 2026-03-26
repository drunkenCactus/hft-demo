#include <gtest/gtest.h>

#include <lib/trade_flow_window.hpp>

using namespace hft;

TEST(TradeFlowWindow, Empty) {
    TradeFlowWindow w;
    EXPECT_EQ(w.AggressiveBuyVolume(), 0U);
    EXPECT_EQ(w.AggressiveSellVolume(), 0U);
}

TEST(TradeFlowWindow, AggressiveVolume) {
    TradeFlowWindow w(1'000'000);
    w.OnTrade(0, false, 100);
    w.OnTrade(0, true, 30);
    EXPECT_EQ(w.AggressiveBuyVolume(), 100U);
    EXPECT_EQ(w.AggressiveSellVolume(), 30U);
}

TEST(TradeFlowWindow, EvictsByExchangeTime) {
    constexpr uint64_t window_us = 1'000;
    TradeFlowWindow w(window_us);

    w.OnTrade(0, false, 100);
    w.OnTrade(500, true, 40);

    // cutoff = 1500 - 1000 = 500 -> drop entry at ts 0
    w.OnTrade(1500, false, 1);
    EXPECT_EQ(w.AggressiveBuyVolume(), 1U);
    EXPECT_EQ(w.AggressiveSellVolume(), 40U);
}

#include <gtest/gtest.h>

#include <lib/local_order_book.hpp>

#include <array>

using namespace hft;

namespace {

constexpr uint32_t DEPTH = 16;

}  // namespace

TEST(OrderBookSide, Ascending_GetBest) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_ASCENDING> empty;
    EXPECT_TRUE(empty.Get().empty());
    EXPECT_EQ(empty.GetBest().price, 0U);
    EXPECT_EQ(empty.GetBest().quantity, 0U);

    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_ASCENDING> side;
    const std::array<uint64_t, 2> prices = {100, 110};
    const std::array<uint64_t, 2> qty = {1, 2};
    side.Init(prices.data(), qty.data(), 2);
    const auto levels = side.Get();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_EQ(side.GetBest().price, levels[0].price);
    EXPECT_EQ(side.GetBest().quantity, levels[0].quantity);
}

TEST(OrderBookSide, Ascending_Init_TruncatesToDepth) {
    OrderBookSide<3, OrderBookSideOrder::PRICE_ASCENDING> side;
    const std::array<uint64_t, 5> prices = {10, 20, 30, 40, 50};
    const std::array<uint64_t, 5> qty = {1, 2, 3, 4, 5};
    side.Init(prices.data(), qty.data(), 5);

    const auto g = side.Get();
    ASSERT_EQ(g.size(), 3U);
    EXPECT_EQ(g[0].price, 10U);
    EXPECT_EQ(g[0].quantity, 1U);
    EXPECT_EQ(g[1].price, 20U);
    EXPECT_EQ(g[1].quantity, 2U);
    EXPECT_EQ(g[2].price, 30U);
    EXPECT_EQ(g[2].quantity, 3U);
}

TEST(OrderBookSide, Ascending_UpdateExistingQuantity) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_ASCENDING> side;
    const std::array<uint64_t, 2> prices = {100, 200};
    const std::array<uint64_t, 2> qty = {1, 2};
    side.Init(prices.data(), qty.data(), 2);

    side.Update(200, 99);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[0].quantity, 1U);
        EXPECT_EQ(g[1].price, 200U);
        EXPECT_EQ(g[1].quantity, 99U);
    }
    side.Update(100, 50);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[0].quantity, 50U);
        EXPECT_EQ(g[1].price, 200U);
        EXPECT_EQ(g[1].quantity, 99U);
    }
}

TEST(OrderBookSide, Ascending_RemoveLevel) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_ASCENDING> side;
    const std::array<uint64_t, 3> prices = {100, 110, 120};
    const std::array<uint64_t, 3> qty = {1, 2, 3};
    side.Init(prices.data(), qty.data(), 3);

    side.Update(110, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[1].price, 120U);
    }
    side.Update(100, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 1U);
        EXPECT_EQ(g[0].price, 120U);
        EXPECT_EQ(g[0].quantity, 3U);
    }
    side.Update(120, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 0U);
    }
    side.Update(1000, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 0U);
    }
}

TEST(OrderBookSide, Ascending_InsertNewLevel) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_ASCENDING> side;

    side.Update(100, 1);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 1U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[0].quantity, 1U);
    }
    side.Update(200, 2);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[1].price, 200U);
        EXPECT_EQ(g[1].quantity, 2U);
    }
    side.Update(150, 7);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 3U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[1].price, 150U);
        EXPECT_EQ(g[1].quantity, 7U);
        EXPECT_EQ(g[2].price, 200U);
    }
}

TEST(OrderBookSide, Descending_UpdateExistingQuantity) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_DESCENDING> side;
    const std::array<uint64_t, 2> prices = {200, 100};
    const std::array<uint64_t, 2> qty = {2, 1};
    side.Init(prices.data(), qty.data(), 2);

    side.Update(200, 42);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 200U);
        EXPECT_EQ(g[0].quantity, 42U);
        EXPECT_EQ(g[1].price, 100U);
    }
    side.Update(100, 5);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[1].price, 100U);
        EXPECT_EQ(g[1].quantity, 5U);
    }
}

TEST(OrderBookSide, Descending_RemoveLevel) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_DESCENDING> side;
    const std::array<uint64_t, 3> prices = {300, 200, 100};
    const std::array<uint64_t, 3> qty = {3, 2, 1};
    side.Init(prices.data(), qty.data(), 3);

    side.Update(200, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 300U);
        EXPECT_EQ(g[1].price, 100U);
    }
    side.Update(300, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 1U);
        EXPECT_EQ(g[0].price, 100U);
        EXPECT_EQ(g[0].quantity, 1U);
    }
    side.Update(100, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 0U);
    }
    side.Update(1000, 0);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 0U);
    }
}

TEST(OrderBookSide, Descending_InsertNewLevel) {
    OrderBookSide<DEPTH, OrderBookSideOrder::PRICE_DESCENDING> side;

    side.Update(200, 2);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 1U);
        EXPECT_EQ(g[0].price, 200U);
        EXPECT_EQ(g[0].quantity, 2U);
    }
    side.Update(100, 1);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 2U);
        EXPECT_EQ(g[0].price, 200U);
        EXPECT_EQ(g[1].price, 100U);
        EXPECT_EQ(g[1].quantity, 1U);
    }
    side.Update(150, 7);
    {
        const auto g = side.Get();
        ASSERT_EQ(g.size(), 3U);
        EXPECT_EQ(g[0].price, 200U);
        EXPECT_EQ(g[1].price, 150U);
        EXPECT_EQ(g[1].quantity, 7U);
        EXPECT_EQ(g[2].price, 100U);
    }
}

TEST(OrderBook, LastUpdateId) {
    OrderBook<DEPTH> book;
    EXPECT_EQ(book.LastUpdateId(), 0U);

    const std::array<uint64_t, 0> empty{};
    book.Init(7, empty.data(), empty.data(), 0, empty.data(), empty.data(), 0);
    EXPECT_EQ(book.LastUpdateId(), 7U);

    book.UpdateBid(100, 1, 1);
    EXPECT_EQ(book.LastUpdateId(), 100U);

    book.UpdateAsk(200, 2, 2);
    EXPECT_EQ(book.LastUpdateId(), 200U);
}

TEST(OrderBook, Init_GetBestBid_GetBestAsk) {
    OrderBook<DEPTH> book;
    const std::array<uint64_t, 2> bid_prices = {105, 100};
    const std::array<uint64_t, 2> bid_qty = {1, 2};
    const std::array<uint64_t, 2> ask_prices = {110, 115};
    const std::array<uint64_t, 2> ask_qty = {3, 4};

    book.Init(1, bid_prices.data(), bid_qty.data(), 2, ask_prices.data(), ask_qty.data(), 2);

    const OrderBookRow bid = book.GetBestBid();
    EXPECT_EQ(bid.price, 105U);
    EXPECT_EQ(bid.quantity, 1U);

    const OrderBookRow ask = book.GetBestAsk();
    EXPECT_EQ(ask.price, 110U);
    EXPECT_EQ(ask.quantity, 3U);

    ASSERT_EQ(book.GetBids().size(), 2U);
    EXPECT_EQ(book.GetBids()[0].price, bid.price);
    ASSERT_EQ(book.GetAsks().size(), 2U);
    EXPECT_EQ(book.GetAsks()[0].price, ask.price);
}

TEST(OrderBook, UpdateBid_UpdateAsk) {
    OrderBook<DEPTH> book;
    const std::array<uint64_t, 1> bid_p = {100};
    const std::array<uint64_t, 1> bid_q = {1};
    const std::array<uint64_t, 1> ask_p = {200};
    const std::array<uint64_t, 1> ask_q = {2};
    book.Init(0, bid_p.data(), bid_q.data(), 1, ask_p.data(), ask_q.data(), 1);

    book.UpdateBid(10, 100, 10);
    book.UpdateAsk(10, 200, 20);

    ASSERT_EQ(book.GetBids().size(), 1U);
    EXPECT_EQ(book.GetBids()[0].price, 100U);
    EXPECT_EQ(book.GetBids()[0].quantity, 10U);
    ASSERT_EQ(book.GetAsks().size(), 1U);
    EXPECT_EQ(book.GetAsks()[0].price, 200U);
    EXPECT_EQ(book.GetAsks()[0].quantity, 20U);
}

TEST(OrderBook, UpdateBid_InsertsNewPriceLevel) {
    OrderBook<DEPTH> book;
    const std::array<uint64_t, 1> bid_p = {100};
    const std::array<uint64_t, 1> bid_q = {1};
    const std::array<uint64_t, 0> empty{};
    book.Init(0, bid_p.data(), bid_q.data(), 1, empty.data(), empty.data(), 0);

    book.UpdateBid(1, 150, 5);

    const auto b = book.GetBids();
    ASSERT_EQ(b.size(), 2U);
    EXPECT_EQ(b[0].price, 150U);
    EXPECT_EQ(b[0].quantity, 5U);
    EXPECT_EQ(b[1].price, 100U);
    EXPECT_EQ(b[1].quantity, 1U);
}

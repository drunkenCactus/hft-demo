#include <gtest/gtest.h>

#include <feeder/parser.hpp>

#include <lib/interprocess/ring_buffer_data.hpp>

#include <vector>

using namespace hft;

// --- ParseDepthEvent ---

TEST(Parser, ParseDepthEvent_Success_InvokesCallbackForEachBidAndAsk) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1672515782136,
        "s": "BTCUSDT",
        "U": 157,
        "u": 160,
        "b": [["0.0024", "10"], ["0.0025", "20"]],
        "a": [["0.0026", "100"], ["0.0027", "200"]]
    })";

    std::vector<OrderBookUpdate> updates;
    bool ok = ParseDepthEvent(json, [&updates](const OrderBookUpdate& u) {
        updates.push_back(u);
    });

    ASSERT_TRUE(ok);
    ASSERT_EQ(updates.size(), 4u);

    EXPECT_EQ(updates[0].type, OrderBookUpdate::Type::BID);
    EXPECT_DOUBLE_EQ(updates[0].price, 0.0024);
    EXPECT_DOUBLE_EQ(updates[0].quantity, 10.0);
    EXPECT_EQ(updates[0].first_update_id, 157u);
    EXPECT_EQ(updates[0].last_update_id, 160u);
    EXPECT_EQ(updates[0].symbol, Symbol::BTCUSDT);

    EXPECT_EQ(updates[1].type, OrderBookUpdate::Type::BID);
    EXPECT_DOUBLE_EQ(updates[1].price, 0.0025);
    EXPECT_DOUBLE_EQ(updates[1].quantity, 20.0);

    EXPECT_EQ(updates[2].type, OrderBookUpdate::Type::ASK);
    EXPECT_DOUBLE_EQ(updates[2].price, 0.0026);
    EXPECT_DOUBLE_EQ(updates[2].quantity, 100.0);

    EXPECT_EQ(updates[3].type, OrderBookUpdate::Type::ASK);
    EXPECT_DOUBLE_EQ(updates[3].price, 0.0027);
    EXPECT_DOUBLE_EQ(updates[3].quantity, 200.0);
}

TEST(Parser, ParseDepthEvent_Success_EventTimeMilliseconds_AutoConverted) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1672515782136,
        "s": "ETHUSDT",
        "U": 1,
        "u": 2,
        "b": [["1.0", "1.0"]],
        "a": []
    })";

    OrderBookUpdate captured;
    bool ok = ParseDepthEvent(json, [&captured](const OrderBookUpdate& u) {
        captured = u;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.meta.event_timestamp_microseconds, 1672515782136000u);
    EXPECT_EQ(captured.symbol, Symbol::ETHUSDT);
}

TEST(Parser, ParseDepthEvent_Success_EventTimeMicroseconds_UsedAsIs) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1672515782136123,
        "s": "ETHUSDT",
        "U": 1,
        "u": 2,
        "b": [["1.0", "1.0"]],
        "a": []
    })";

    OrderBookUpdate captured;
    bool ok = ParseDepthEvent(json, [&captured](const OrderBookUpdate& u) {
        captured = u;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.meta.event_timestamp_microseconds, 1672515782136123u);
    EXPECT_EQ(captured.symbol, Symbol::ETHUSDT);
}

TEST(Parser, ParseDepthEvent_Success_EmptyBidsAndAsks_ReturnsTrue) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1,
        "s": "BTCUSDT",
        "U": 0,
        "u": 0,
        "b": [],
        "a": []
    })";

    std::vector<OrderBookUpdate> updates;
    bool ok = ParseDepthEvent(json, [&updates](const OrderBookUpdate& u) {
        updates.push_back(u);
    });

    ASSERT_TRUE(ok);
    EXPECT_TRUE(updates.empty());
}

TEST(Parser, ParseDepthEvent_Success_UnknownSymbol) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1,
        "s": "XRPUSDT",
        "U": 0,
        "u": 0,
        "b": [["1", "1"]],
        "a": []
    })";

    OrderBookUpdate captured;
    bool ok = ParseDepthEvent(json, [&captured](const OrderBookUpdate& u) {
        captured = u;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.symbol, Symbol::UNKNOWN);
}

TEST(Parser, ParseDepthEvent_Failure_InvalidJson) {
    const std::string json = "{ invalid json ";
    std::vector<OrderBookUpdate> updates;
    bool ok = ParseDepthEvent(json, [&updates](const OrderBookUpdate& u) {
        updates.push_back(u);
    });
    EXPECT_FALSE(ok);
    EXPECT_TRUE(updates.empty());
}

TEST(Parser, ParseDepthEvent_Failure_EmptyString) {
    const std::string json = "";
    bool ok = ParseDepthEvent(json, [](const OrderBookUpdate&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseDepthEvent_Failure_WrongEventType) {
    const std::string json = R"({"e": "trade", "E": 1, "s": "BTCUSDT"})";
    bool ok = ParseDepthEvent(json, [](const OrderBookUpdate&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseDepthEvent_Failure_NotAnObject) {
    const std::string json = "[]";
    bool ok = ParseDepthEvent(json, [](const OrderBookUpdate&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseDepthEvent_NullCallback_ReturnsTrueForValidJson) {
    const std::string json = R"({
        "e": "depthUpdate",
        "E": 1,
        "s": "BTCUSDT",
        "U": 0,
        "u": 0,
        "b": [["1", "1"]],
        "a": []
    })";
    bool ok = ParseDepthEvent(json, nullptr);
    EXPECT_TRUE(ok);
}

// --- ParseTradeEvent ---

TEST(Parser, ParseTradeEvent_Success_EventTimeMilliseconds_AutoConverted) {
    const std::string json = R"({
        "e": "trade",
        "E": 1672515782136,
        "s": "BTCUSDT",
        "t": 12345,
        "p": "0.001",
        "q": "100",
        "T": 1672515782136,
        "m": true,
        "M": true
    })";

    Trade captured;
    bool ok = ParseTradeEvent(json, [&captured](const Trade& t) {
        captured = t;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.symbol, Symbol::BTCUSDT);
    EXPECT_DOUBLE_EQ(captured.price, 0.001);
    EXPECT_DOUBLE_EQ(captured.quantity, 100.0);
    EXPECT_EQ(captured.meta.event_timestamp_microseconds, 1672515782136000u);
}

TEST(Parser, ParseTradeEvent_Success_EventTimeMicroseconds_UsedAsIs) {
    const std::string json = R"({
        "e": "trade",
        "E": 1672515782136123,
        "s": "BTCUSDT",
        "p": "0.001",
        "q": "100"
    })";

    Trade captured;
    bool ok = ParseTradeEvent(json, [&captured](const Trade& t) {
        captured = t;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.symbol, Symbol::BTCUSDT);
    EXPECT_DOUBLE_EQ(captured.price, 0.001);
    EXPECT_DOUBLE_EQ(captured.quantity, 100.0);
    EXPECT_EQ(captured.meta.event_timestamp_microseconds, 1672515782136123u);
}

TEST(Parser, ParseTradeEvent_Success_ETHUSDT) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "ETHUSDT",
        "p": "2500.5",
        "q": "0.25"
    })";

    Trade captured;
    bool ok = ParseTradeEvent(json, [&captured](const Trade& t) {
        captured = t;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.symbol, Symbol::ETHUSDT);
    EXPECT_DOUBLE_EQ(captured.price, 2500.5);
    EXPECT_DOUBLE_EQ(captured.quantity, 0.25);
}

TEST(Parser, ParseTradeEvent_Failure_InvalidJson) {
    const std::string json = "{ broken ";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_Failure_WrongEventType) {
    const std::string json = R"({"e": "depthUpdate", "E": 1, "s": "BTCUSDT"})";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_Failure_MissingPrice) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "BTCUSDT",
        "q": "100"
    })";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_Failure_MissingQuantity) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "BTCUSDT",
        "p": "0.001"
    })";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_Failure_InvalidPrice) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "BTCUSDT",
        "p": "not_a_number",
        "q": "100"
    })";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_Failure_InvalidQuantity) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "BTCUSDT",
        "p": "0.001",
        "q": "abc"
    })";
    bool ok = ParseTradeEvent(json, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseTradeEvent_NullCallback_ReturnsTrueForValidJson) {
    const std::string json = R"({
        "e": "trade",
        "E": 1,
        "s": "BTCUSDT",
        "p": "0.001",
        "q": "100"
    })";
    bool ok = ParseTradeEvent(json, nullptr);
    EXPECT_TRUE(ok);
}

// --- ParseEvent (combined stream wrapper) ---

TEST(Parser, ParseEvent_DepthStream_InvokesOrderBookCallback) {
    const std::string json = R"({
        "stream": "btcusdt@depth@100ms",
        "data": {
            "e": "depthUpdate",
            "E": 1672515782136,
            "s": "BTCUSDT",
            "U": 157,
            "u": 160,
            "b": [["0.0024", "10"]],
            "a": [["0.0026", "100"]]
        }
    })";

    std::vector<OrderBookUpdate> updates;
    Trade trade_captured;
    bool trade_called = false;
    bool ok = ParseEvent(
        json,
        [&updates](const OrderBookUpdate& u) { updates.push_back(u); },
        [&trade_captured, &trade_called](const Trade& t) {
            trade_captured = t;
            trade_called = true;
        });

    ASSERT_TRUE(ok);
    ASSERT_EQ(updates.size(), 2u);
    EXPECT_FALSE(trade_called);
    EXPECT_EQ(updates[0].type, OrderBookUpdate::Type::BID);
    EXPECT_DOUBLE_EQ(updates[0].price, 0.0024);
    EXPECT_DOUBLE_EQ(updates[0].quantity, 10.0);
    EXPECT_EQ(updates[1].type, OrderBookUpdate::Type::ASK);
    EXPECT_DOUBLE_EQ(updates[1].price, 0.0026);
    EXPECT_DOUBLE_EQ(updates[1].quantity, 100.0);
}

TEST(Parser, ParseEvent_DepthStreamBtcusdtAtDepth_Equivalent) {
    const std::string json = R"({
        "stream": "btcusdt@depth",
        "data": {
            "e": "depthUpdate",
            "E": 1,
            "s": "BTCUSDT",
            "U": 0,
            "u": 0,
            "b": [["1.0", "2.0"]],
            "a": []
        }
    })";

    std::vector<OrderBookUpdate> updates;
    bool ok = ParseEvent(
        json,
        [&updates](const OrderBookUpdate& u) { updates.push_back(u); },
        [](const Trade&) {});

    ASSERT_TRUE(ok);
    ASSERT_EQ(updates.size(), 1u);
    EXPECT_DOUBLE_EQ(updates[0].price, 1.0);
    EXPECT_DOUBLE_EQ(updates[0].quantity, 2.0);
}

TEST(Parser, ParseEvent_TradeStream_InvokesTradeCallback) {
    const std::string json = R"({
        "stream": "btcusdt@trade",
        "data": {
            "e": "trade",
            "E": 1672515782136,
            "s": "BTCUSDT",
            "p": "0.001",
            "q": "100"
        }
    })";

    std::vector<OrderBookUpdate> depth_updates;
    Trade trade_captured;
    bool depth_called = false;
    bool ok = ParseEvent(
        json,
        [&depth_called](const OrderBookUpdate&) { depth_called = true; },
        [&trade_captured](const Trade& t) { trade_captured = t; });

    ASSERT_TRUE(ok);
    EXPECT_FALSE(depth_called);
    EXPECT_EQ(trade_captured.symbol, Symbol::BTCUSDT);
    EXPECT_DOUBLE_EQ(trade_captured.price, 0.001);
    EXPECT_DOUBLE_EQ(trade_captured.quantity, 100.0);
}

TEST(Parser, ParseEvent_OtherInstrument_AcceptedBySuffix) {
    const std::string json = R"({
        "stream": "ethusdt@trade",
        "data": {
            "e": "trade",
            "E": 1,
            "s": "ETHUSDT",
            "p": "2500",
            "q": "1"
        }
    })";

    Trade trade_captured;
    bool ok = ParseEvent(
        json,
        [](const OrderBookUpdate&) {},
        [&trade_captured](const Trade& t) { trade_captured = t; });

    ASSERT_TRUE(ok);
    EXPECT_EQ(trade_captured.symbol, Symbol::ETHUSDT);
    EXPECT_DOUBLE_EQ(trade_captured.price, 2500.0);
    EXPECT_DOUBLE_EQ(trade_captured.quantity, 1.0);
}

TEST(Parser, ParseEvent_Failure_InvalidJson) {
    const std::string json = "{ invalid ";
    bool ok = ParseEvent(json, [](const OrderBookUpdate&) {}, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseEvent_Failure_MissingStream) {
    const std::string json = R"({"data": {"e": "trade", "p": "1", "q": "1"}})";
    bool ok = ParseEvent(json, [](const OrderBookUpdate&) {}, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseEvent_Failure_MissingData) {
    const std::string json = R"({"stream": "btcusdt@trade"})";
    bool ok = ParseEvent(json, [](const OrderBookUpdate&) {}, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseEvent_Failure_UnknownStream) {
    const std::string json = R"({
        "stream": "btcusdt@aggTrade",
        "data": {}
    })";
    bool ok = ParseEvent(json, [](const OrderBookUpdate&) {}, [](const Trade&) {});
    EXPECT_FALSE(ok);
}

// --- ParseOrderBookSnapshot ---

TEST(Parser, ParseOrderBookSnapshot_Success) {
    const std::string json = R"({
        "lastUpdateId": 1027024,
        "bids": [
            ["4.00000000", "431.00000000"],
            ["3.50000000", "100.0"]
        ],
        "asks": [
            ["4.00000200", "12.00000000"],
            ["4.00000300", "50.0"]
        ]
    })";

    OrderBookSnapshot captured;
    bool ok = ParseOrderBookSnapshot(json, [&captured](const OrderBookSnapshot& s) {
        captured = s;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.last_update_id, 1027024u);
    EXPECT_EQ(captured.bids_depth, 2u);
    EXPECT_EQ(captured.asks_depth, 2u);

    EXPECT_DOUBLE_EQ(captured.bids_prices[0], 4.0);
    EXPECT_DOUBLE_EQ(captured.bids_quantities[0], 431.0);
    EXPECT_DOUBLE_EQ(captured.bids_prices[1], 3.5);
    EXPECT_DOUBLE_EQ(captured.bids_quantities[1], 100.0);

    EXPECT_DOUBLE_EQ(captured.asks_prices[0], 4.000002);
    EXPECT_DOUBLE_EQ(captured.asks_quantities[0], 12.0);
    EXPECT_DOUBLE_EQ(captured.asks_prices[1], 4.000003);
    EXPECT_DOUBLE_EQ(captured.asks_quantities[1], 50.0);
}

TEST(Parser, ParseOrderBookSnapshot_Success_EmptyBidsAndAsks) {
    const std::string json = R"({
        "lastUpdateId": 1,
        "bids": [],
        "asks": []
    })";

    OrderBookSnapshot captured;
    bool ok = ParseOrderBookSnapshot(json, [&captured](const OrderBookSnapshot& s) {
        captured = s;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.last_update_id, 1u);
    EXPECT_EQ(captured.bids_depth, 0u);
    EXPECT_EQ(captured.asks_depth, 0u);
}

TEST(Parser, ParseOrderBookSnapshot_Success_NoLastUpdateId_DefaultsToZero) {
    const std::string json = R"({
        "bids": [["1", "1"]],
        "asks": []
    })";

    OrderBookSnapshot captured;
    bool ok = ParseOrderBookSnapshot(json, [&captured](const OrderBookSnapshot& s) {
        captured = s;
    });

    ASSERT_TRUE(ok);
    EXPECT_EQ(captured.last_update_id, 0u);
    EXPECT_EQ(captured.bids_depth, 1u);
    EXPECT_EQ(captured.asks_depth, 0u);
}

TEST(Parser, ParseOrderBookSnapshot_Failure_InvalidJson) {
    const std::string json = "{ ] invalid ";
    bool ok = ParseOrderBookSnapshot(json, [](const OrderBookSnapshot&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseOrderBookSnapshot_Failure_NotAnObject) {
    const std::string json = "null";
    bool ok = ParseOrderBookSnapshot(json, [](const OrderBookSnapshot&) {});
    EXPECT_FALSE(ok);
}

TEST(Parser, ParseOrderBookSnapshot_NullCallback_ReturnsTrueForValidJson) {
    const std::string json = R"({
        "lastUpdateId": 1,
        "bids": [["1", "1"]],
        "asks": []
    })";
    bool ok = ParseOrderBookSnapshot(json, nullptr);
    EXPECT_TRUE(ok);
}

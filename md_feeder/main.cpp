#include <lib/logger.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <rapidjson/document.h>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using tcp = net::ip::tcp;

const std::string HOST = "stream.binance.com";
const std::string PORT = "9443";
const std::string TARGET = "/ws/btcusdt@depth5@100ms";
const std::string LOGFILE_PATH = "/var/log/hft/md_feeder.log";

Logger<LogLevel::INFO> LOG_INFO(LOGFILE_PATH);
Logger<LogLevel::ERROR> LOG_ERROR(LOGFILE_PATH);

struct OrderBook {
    std::vector<std::pair<double, double>> bids, asks;
    std::mutex mtx;

    void update(const rapidjson::Value& bids_arr, const rapidjson::Value& asks_arr) {
        std::lock_guard<std::mutex> lock(mtx);
        bids.clear(); asks.clear();
        for (auto& v : bids_arr.GetArray()) {
            bids.emplace_back(
                std::stod(v[0].GetString()),
                std::stod(v[1].GetString())
            );
        }
        for (auto& v : asks_arr.GetArray()) {
            asks.emplace_back(
                std::stod(v[0].GetString()),
                std::stod(v[1].GetString())
            );
        }
    }
    std::pair<double, double> best_bid() {
        std::lock_guard<std::mutex> lock(mtx);
        return bids.empty() ? std::make_pair(0.0,0.0) : bids.front();
    }
    std::pair<double, double> best_ask() {
        std::lock_guard<std::mutex> lock(mtx);
        return asks.empty() ? std::make_pair(0.0,0.0) : asks.front();
    }
};

OrderBook book;
std::atomic<bool> running{true};

void StrategyLoop() {
    while (running) {
        auto [bid_px, bid_qty] = book.best_bid();
        auto [ask_px, ask_qty] = book.best_ask();
        if (bid_px == 0 || ask_px == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        double spread = ask_px - bid_px;
        if (spread >= 0.2) {
            double buy_price = bid_px + 0.01;
            double sell_price = ask_px - 0.01;
            double order_qty = 0.001;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            LOG_INFO << now << ",BUY," << buy_price << "," << order_qty << Endl;
            LOG_INFO << now << ",SELL," << sell_price << "," << order_qty << Endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void OnWsMessage(const std::string& text) {
    rapidjson::Document d;
    d.Parse(text.c_str());
    if (d.HasParseError()) {
        LOG_ERROR << "Parsing error!" << Endl;
        return;
    }
    if (d.HasMember("bids") && d.HasMember("asks")) {
        book.update(d["bids"], d["asks"]);
    }
}

int main() {
    LOG_INFO << "Starting MD Feeder!" << Endl;
    std::thread strategy_thread(StrategyLoop);

    net::io_context ioc;
    ssl::context ctx(ssl::context::tlsv12_client);

    ctx.set_verify_mode(ssl::verify_none);

    try {
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(HOST, PORT);

        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        beast::get_lowest_layer(stream).connect(results);

        stream.handshake(ssl::stream_base::client);

        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(std::move(stream));

        ws.handshake(HOST, TARGET);

        LOG_INFO << "WebSocket connected!" << Endl;

        while (running) {
            beast::flat_buffer buffer;
            ws.read(buffer);

            auto msg = beast::buffers_to_string(buffer.data());
            OnWsMessage(msg);
        }

    } catch (std::exception& e) {
        LOG_ERROR << "Exception: " << e.what() << Endl;
        running = false;
    }

    running = false;
    strategy_thread.join();
    LOG_INFO << "Terminated" << Endl;
    return 0;
}

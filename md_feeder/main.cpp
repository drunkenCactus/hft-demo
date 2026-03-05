#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

#include <rapidjson/document.h>

#include <chrono>
#include <mutex>
#include <vector>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using tcp = net::ip::tcp;

const std::string HOST = "stream.binance.com";
const std::string PORT = "9443";
const std::string TARGET = "/ws/btcusdt@depth5@100ms";

class OrderBook {
public:
    void Update(const rapidjson::Value& bids_arr, const rapidjson::Value& asks_arr) {
        std::lock_guard<std::mutex> lock(mtx_);
        bids_.clear(); asks_.clear();
        for (auto& v : bids_arr.GetArray()) {
            bids_.emplace_back(
                std::stod(v[0].GetString()),
                std::stod(v[1].GetString())
            );
        }
        for (auto& v : asks_arr.GetArray()) {
            asks_.emplace_back(
                std::stod(v[0].GetString()),
                std::stod(v[1].GetString())
            );
        }
    }
    std::pair<double, double> BestBid() {
        std::lock_guard<std::mutex> lock(mtx_);
        return bids_.empty() ? std::make_pair(0.0,0.0) : bids_.front();
    }

    std::pair<double, double> BestAsk() {
        std::lock_guard<std::mutex> lock(mtx_);
        return asks_.empty() ? std::make_pair(0.0,0.0) : asks_.front();
    }

private:
    std::vector<std::pair<double, double>> bids_;
    std::vector<std::pair<double, double>> asks_;
    std::mutex mtx_;
};

void OnWsMessage(OrderBook& book, const std::string& text, hft::BestBidAskRingBufferData& shared_data) {
    rapidjson::Document d;
    d.Parse(text.c_str());
    if (d.HasParseError()) {
        HOT_ERROR << "Parsing error!" << Endl;
        return;
    }
    if (d.HasMember("bids") && d.HasMember("asks")) {
        book.Update(d["bids"], d["asks"]);
        shared_data.best_bid = book.BestBid();
        shared_data.best_ask = book.BestAsk();
        shared_data.ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

int main() {
    try {
        hft::RemoveSharedMemory(hft::SHM_NAME_MD_FEEDER_TO_OBSERVER);
        hft::ShmToObserver shm_log(hft::SHM_NAME_MD_FEEDER_TO_OBSERVER, hft::MemoryRole::CREATE_ONLY);
        auto [ring_buffer_log] = shm_log.GetObjects();

        hft::HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "MD Feeder started!" << Endl;

        hft::RemoveSharedMemory(hft::SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE);
        hft::ShmMdFeederToTradingEngine shared_memory(hft::SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE, hft::MemoryRole::CREATE_ONLY);
        auto [ring_buffer] = shared_memory.GetObjects();

        net::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_verify_mode(ssl::verify_none);

        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(HOST, PORT);

        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(std::move(stream));
        ws.handshake(HOST, TARGET);
        HOT_INFO << "WebSocket connected!" << Endl;

        OrderBook book;
        while (true) {
            beast::flat_buffer buffer;
            ws.read(buffer);

            auto msg = beast::buffers_to_string(buffer.data());
            hft::BestBidAskRingBufferData shared_data;
            OnWsMessage(book, msg, shared_data);
            ring_buffer->Write(shared_data);
        }

    } catch (const std::exception& e) {
        HOT_ERROR << "Exception: " << e.what() << Endl;
        return 1;
    }

    return 0;
}

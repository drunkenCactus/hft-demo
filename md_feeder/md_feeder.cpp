#include "binance_client.hpp"

#include <lib/interprocess/hot_path_logger.hpp>
#include <lib/interprocess/interprocess.hpp>

#include <rapidjson/document.h>

namespace hft {

namespace {

const std::string HOST = "stream.binance.com";
const std::string PORT = "9443";
const std::string TARGET = "/ws/btcusdt@depth5@100ms";

class OrderBook {
public:
    bool Parse(const std::string& raw_data) {
        rapidjson::Document json_struct;
        json_struct.Parse(raw_data.c_str());
        if (json_struct.HasParseError()) {
            return false;
        }

        if (json_struct.HasMember("bids") &&  json_struct["bids"].IsArray() &&
            json_struct.HasMember("asks") &&  json_struct["asks"].IsArray()
        ) {
            bids_.clear();
            asks_.clear();
            for (const auto& v : json_struct["bids"].GetArray()) {
                bids_.emplace_back(
                    std::stod(v[0].GetString()),
                    std::stod(v[1].GetString())
                );
            }
            for (const auto& v : json_struct["asks"].GetArray()) {
                asks_.emplace_back(
                    std::stod(v[0].GetString()),
                    std::stod(v[1].GetString())
                );
            }
            return true;
        } else {
            return false;
        }
    }

    void FillBestBidAskData(BestBidAskRingBufferData& data) const noexcept {
        data.best_bid = bids_.empty() ? std::pair<double, double>{0.0, 0.0} : bids_.front();
        data.best_ask = asks_.empty() ? std::pair<double, double>{0.0, 0.0} : asks_.front();
    }

private:
    std::vector<std::pair<double, double>> bids_;
    std::vector<std::pair<double, double>> asks_;
};

}  // namespace

int RunMdFeeder() {
    try {
        RemoveSharedMemory(SHM_NAME_MD_FEEDER_TO_OBSERVER);
        ShmToObserver shm_log(SHM_NAME_MD_FEEDER_TO_OBSERVER, MemoryRole::CREATE_ONLY);
        shm_log.UpdateHeartbeat();
        auto [ring_buffer_log] = shm_log.GetObjects();

        HotPathLogger::Init(ring_buffer_log);
        HOT_INFO << "MD Feeder started!" << Endl;

        RemoveSharedMemory(SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE);
        ShmMdFeederToTradingEngine shm_market_data(SHM_NAME_MD_FEEDER_TO_TRADING_ENGINE, MemoryRole::CREATE_ONLY);
        shm_market_data.UpdateHeartbeat();
        auto [ring_buffer_md] = shm_market_data.GetObjects();
        HOT_INFO << "Market Data shared memory created" << Endl;

        BinanceWsClient client(HOST, PORT, TARGET);
        HOT_INFO << "Binance websocket connected" << Endl;

        OrderBook book;
        BestBidAskRingBufferData data;

        while (true) {
            shm_log.UpdateHeartbeat();
            shm_market_data.UpdateHeartbeat();

            std::string message = client.Read();
            if (book.Parse(message)) {
                book.FillBestBidAskData(data);
                ring_buffer_md->Write(data);
            } else {
                HOT_WARNING << "Parsing error";
            }
        }

    } catch (const std::exception& e) {
        HOT_ERROR << "Exception: " << e.what() << Endl;
        return 1;
    }

    HOT_INFO << "Finishing MD Feeder" << Endl;
    return 0;
}

}  // namespace hft

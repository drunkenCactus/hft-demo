#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

namespace hft {

class BinanceWsClient {
public:
    BinanceWsClient(const std::string& host, const std::string& port, const std::string& target);

    BinanceWsClient(const BinanceWsClient&) = delete;
    BinanceWsClient(BinanceWsClient&&) = delete;
    BinanceWsClient& operator=(const BinanceWsClient&) = delete;
    BinanceWsClient& operator=(BinanceWsClient&&) = delete;

    ~BinanceWsClient();

    std::string Read();

private:
    using SslStream = boost::beast::ssl_stream<boost::beast::tcp_stream>;

    boost::beast::websocket::stream<SslStream> CreateWebsocket(const std::string& host, const std::string& port);

private:
    boost::beast::websocket::stream<SslStream> websocket_;
};

}  // namespace hft

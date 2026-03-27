#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

#include <cstddef>
#include <string_view>

namespace hft {

// max WebSocket message size
constexpr std::size_t kBinanceWsReadBufferSize = 512 * 1024;

class BinanceWsClient {
public:
    BinanceWsClient(std::string_view host, std::string_view port, std::string_view target);

    BinanceWsClient(const BinanceWsClient&) = delete;
    BinanceWsClient(BinanceWsClient&&) = delete;
    BinanceWsClient& operator=(const BinanceWsClient&) = delete;
    BinanceWsClient& operator=(BinanceWsClient&&) = delete;

    ~BinanceWsClient();

    std::string_view Read();

private:
    using SslStream = boost::beast::ssl_stream<boost::beast::tcp_stream>;

    boost::beast::websocket::stream<SslStream> CreateWebsocket(
        std::string_view host,
        std::string_view port
    );

private:
    boost::beast::websocket::stream<SslStream> websocket_;
    boost::beast::flat_static_buffer<kBinanceWsReadBufferSize> read_buffer_;
};

}  // namespace hft

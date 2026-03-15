#include "binance_api_client.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl.hpp>

#include <cstdio>
#include <stdexcept>

namespace hft {

BinanceApiClient::BinanceApiClient(std::string_view symbol, uint32_t limit)
    : target_(std::format("/api/v3/depth?symbol={}&limit={}", symbol, limit))
{}

std::string_view BinanceApiClient::GetOrderBookShapshot() {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(boost::asio::ssl::verify_none);

    boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ctx);
    boost::asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host_, port_);
    boost::beast::get_lowest_layer(stream).connect(results);
    stream.handshake(boost::asio::ssl::stream_base::client);

    boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::get, target_, 11};
    req.set(boost::beast::http::field::host, host_);
    req.set(boost::beast::http::field::connection, "close");

    write(stream, req);

    boost::beast::http::response_parser<boost::beast::http::buffer_body> parser;
    parser.body_limit(body_buffer_.size());
    parser.get().body().data = body_buffer_.data();
    parser.get().body().size = body_buffer_.size();

    read_buf_.consume(read_buf_.size());
    read(stream, read_buf_, parser);

    if (!parser.is_done()) {
        throw std::runtime_error("binance depth: response body larger than buffer");
    }

    uint32_t body_len = body_buffer_.size() - parser.get().body().size;
    return std::string_view(body_buffer_.data(), body_len);
}

}  // namespace hft

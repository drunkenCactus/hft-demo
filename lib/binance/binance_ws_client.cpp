#include "binance_ws_client.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core/buffers_range.hpp>

#include <string>

namespace hft {

BinanceWsClient::BinanceWsClient(std::string_view host, std::string_view port, std::string_view target)
    : websocket_(CreateWebsocket(host, port))
{
    websocket_.handshake(std::string(host), std::string(target));
}

BinanceWsClient::~BinanceWsClient() {
    websocket_.close(boost::beast::websocket::close_code::normal);
}

std::string_view BinanceWsClient::Read() {
    read_buffer_.consume(read_buffer_.size());
    websocket_.read(read_buffer_);
    auto buf = boost::beast::buffers_front(read_buffer_.data());
    return std::string_view(static_cast<const char*>(buf.data()), buf.size());
}

boost::beast::websocket::stream<BinanceWsClient::SslStream> BinanceWsClient::CreateWebsocket(
    std::string_view host,
    std::string_view port
) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(boost::asio::ssl::verify_none);
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(std::string(host), std::string(port));

    SslStream stream(ioc, ctx);
    boost::beast::get_lowest_layer(stream).connect(results);
    stream.handshake(boost::asio::ssl::stream_base::client);

    return boost::beast::websocket::stream<SslStream>(std::move(stream));
}

}  // namespace hft

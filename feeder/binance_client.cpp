#include "binance_client.hpp"

#include <boost/asio.hpp>

namespace hft {

BinanceWsClient::BinanceWsClient(const std::string& host, const std::string& port, const std::string& target)
    : websocket_(CreateWebsocket(host, port))
{
    websocket_.handshake(host, target);
}

BinanceWsClient::~BinanceWsClient() {
    websocket_.close(boost::beast::websocket::close_code::normal);
}

std::string BinanceWsClient::Read() {
    boost::beast::flat_buffer buffer;
    websocket_.read(buffer);
    return boost::beast::buffers_to_string(buffer.cdata());
}

boost::beast::websocket::stream<BinanceWsClient::SslStream> BinanceWsClient::CreateWebsocket(const std::string& host, const std::string& port) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(boost::asio::ssl::verify_none);
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, port);

    SslStream stream(ioc, ctx);
    boost::beast::get_lowest_layer(stream).connect(results);
    stream.handshake(boost::asio::ssl::stream_base::client);

    return boost::beast::websocket::stream<SslStream>(std::move(stream));
}

}  // namespace hft

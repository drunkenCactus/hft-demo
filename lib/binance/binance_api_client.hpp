#pragma once

#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace hft {

class BinanceApiClient {
public:
    explicit BinanceApiClient(std::string_view symbol, uint32_t limit);

    BinanceApiClient(const BinanceApiClient&) = delete;
    BinanceApiClient(BinanceApiClient&&) = delete;
    BinanceApiClient& operator=(const BinanceApiClient&) = delete;
    BinanceApiClient& operator=(BinanceApiClient&&) = delete;

    std::string_view GetOrderBookShapshot();

private:
    static constexpr std::size_t HTTP_READ_BUFFER_SIZE = 64 * 1024;
    static constexpr std::size_t BODY_BUFFER_SIZE = 1024 * 1024;

    const std::string host_ = "api.binance.com";
    const std::string port_ = "443";
    const std::string target_;

    std::array<char, BODY_BUFFER_SIZE> body_buffer_;
    boost::beast::flat_static_buffer<HTTP_READ_BUFFER_SIZE> read_buf_;
};

}  // namespace hft

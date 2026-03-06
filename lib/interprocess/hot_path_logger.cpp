#include "hot_path_logger.hpp"

namespace hft {

HotPathLogger::Formatter::Formatter(HotPathLogger& logger, LogLevel level) noexcept
    : logger_(logger)
{
    data_.level = level;
    data_.timestamp_ns = NowNanoseconds();
}

HotPathLogger::Formatter& HotPathLogger::Formatter::operator<<(const char* str) noexcept {
    while (*str && msg_pos_ < data_.message + true_size_) {
        *msg_pos_++ = *str++;
    }
    return *this;
}

HotPathLogger::Formatter& HotPathLogger::Formatter::operator<<(double value) noexcept {
    auto result = std::to_chars(msg_pos_, data_.message + true_size_, value, std::chars_format::fixed);
    if (result.ec == std::errc()) {
        msg_pos_ = result.ptr;
    }
    return *this;
}

HotPathLogger::Formatter& HotPathLogger::Formatter::operator<<(const TypeEndl&) noexcept {
    *msg_pos_ = '\0';
    logger_.Write(data_);
    msg_pos_ = data_.message;
    return *this;
}

void HotPathLogger::Write(const ObserverRingBufferData& data) noexcept {
    ring_buffer_->Write(data);
}

void HotPathLogger::Create(ObserverRingBuffer* ring_buffer) noexcept {
    ring_buffer_ = ring_buffer;
}

HotPathLogger::Formatter HotPathLogger::GetFormatter(LogLevel level) noexcept {
    return HotPathLogger::Formatter(*this, level);
}

HotPathLogger& HotPathLogger::Instance() noexcept {
    static HotPathLogger logger;
    return logger;
}

void HotPathLogger::Init(ObserverRingBuffer* ring_buffer) noexcept {
    Instance().Create(ring_buffer);
}

}  // namespace hft

#pragma once

#include <lib/interprocess/interprocess.hpp>
#include <lib/log_common.hpp>

#include <algorithm>

namespace hft {

class HotPathLogger {
private:
    class Formatter {
    public:
        Formatter(HotPathLogger& logger, LogLevel level) noexcept;

        Formatter(const Formatter& other) = delete;
        Formatter(Formatter&& other) = delete;
        Formatter& operator=(const Formatter& other) = delete;
        Formatter& operator=(Formatter&& other) = delete;
        ~Formatter() = default;

        template<uint32_t N>
        Formatter& operator<<(const char (&str)[N]) noexcept {
            const uint32_t free_space = true_size_ - (msg_pos_ - data_.message);
            const uint32_t count = std::min(N - 1, free_space);
            memcpy(msg_pos_, str, count);
            msg_pos_ += count;
            return *this;
        }

        Formatter& operator<<(const char* str) noexcept;

        template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
        Formatter& operator<<(T value) noexcept {
            auto result = std::to_chars(msg_pos_, data_.message + true_size_, value);
            if (result.ec == std::errc()) {
                msg_pos_ = result.ptr;
            }
            return *this;
        }

        Formatter& operator<<(double value) noexcept;

        Formatter& operator<<(const TypeEndl&) noexcept;

    private:
        HotPathLogger& logger_;
        ObserverData data_;
        char* msg_pos_ = data_.message;

        constexpr static uint32_t true_size_ = ObserverData::message_size - 1;
        static_assert(true_size_ > 0);
    };

public:
    HotPathLogger() = default;
    HotPathLogger(const HotPathLogger& other) = delete;
    HotPathLogger(HotPathLogger&& other) = delete;
    HotPathLogger& operator=(const HotPathLogger& other) = delete;
    HotPathLogger& operator=(HotPathLogger&& other) = delete;
    ~HotPathLogger() = default;

    void Write(const ObserverData& data) noexcept;

    void Create(ObserverRingBuffer* ring_buffer) noexcept;

    Formatter GetFormatter(LogLevel level) noexcept;

    static HotPathLogger& Instance() noexcept;

    static void Init(ObserverRingBuffer* ring_buffer) noexcept;

private:
    ObserverRingBuffer* ring_buffer_ = nullptr;
};

}  // namespace hft

#define HOT_INFO hft::HotPathLogger::Instance().GetFormatter(hft::LogLevel::kInfo)
#define HOT_WARNING hft::HotPathLogger::Instance().GetFormatter(hft::LogLevel::kWarning)
#define HOT_ERROR hft::HotPathLogger::Instance().GetFormatter(hft::LogLevel::kError)

#pragma once

#include <lib/log_common.hpp>

#include <fstream>
#include <mutex>
#include <sstream>

namespace hft {

class Logger {
private:
    class Formatter {
    public:
        Formatter(Logger& logger, LogLevel level);

        Formatter(const Formatter& other) = delete;
        Formatter(Formatter&& other) = delete;
        Formatter& operator=(const Formatter& other) = delete;
        Formatter& operator=(Formatter&& other) = delete;
        ~Formatter() = default;

        template <typename T>
        Formatter& operator<<(const T& data) {
            buffer_ << data;
            return *this;
        }

        Formatter& operator<<(const TypeEndl&);

    private:
        Logger& logger_;
        const LogLevel level_;
        std::ostringstream buffer_;
    };

public:
    Logger() = default;
    Logger(const Logger& other) = delete;
    Logger(Logger&& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    Logger& operator=(Logger&& other) = delete;

    ~Logger();

    void Open(const std::string logfile_path);

    void Write(const std::string& data, LogLevel level);

    Formatter GetFormatter(LogLevel level) noexcept;

    static Logger& Instance() noexcept;

    static void Init(const std::string logfile_path);

private:
    std::mutex mutex_;
    std::ofstream logfile_;
};

}  // namespace hft

#define LOG_INFO hft::Logger::Instance().GetFormatter(hft::LogLevel::INFO)
#define LOG_WARNING hft::Logger::Instance().GetFormatter(hft::LogLevel::WARNING)
#define LOG_ERROR hft::Logger::Instance().GetFormatter(hft::LogLevel::ERROR)

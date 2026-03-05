#include "logger.hpp"

#include <chrono>
#include <format>

namespace hft {

Logger::Formatter::Formatter(Logger& logger, LogLevel level)
    : logger_(logger)
    , level_(level) {}

Logger::Formatter& Logger::Formatter::operator<<(const TypeEndl&) {
    logger_.Write(buffer_.str(), level_);
    buffer_.str({});
    buffer_.clear();
    return *this;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    logfile_.close();
}

void Logger::Open(const std::string logfile_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    logfile_.open(logfile_path, std::ios::app);
    if (!logfile_.is_open()) {
        throw std::runtime_error("Сannot open logfile " + logfile_path);
    }
}

void Logger::Write(const std::string& data, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logfile_ << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now()) << " ";
    switch (level) {
        case LogLevel::INFO:
            logfile_ << "[INFO] ";
            break;
        case LogLevel::WARNING:
            logfile_ << "[WARNING] ";
            break;
        case LogLevel::ERROR:
            logfile_ << "[ERROR] ";
            break;
    }
    logfile_ << data << std::endl;
}

Logger::Formatter Logger::GetFormatter(LogLevel level) noexcept {
    return Formatter(*this, level);
}

Logger& Logger::Instance() noexcept {
    static Logger logger;
    return logger;
}

void Logger::Init(const std::string logfile_path) {
    Instance().Open(logfile_path);
}

}  // namespace hft

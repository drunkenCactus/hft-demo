#pragma once

#include <chrono>
#include <format>
#include <fstream>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

constexpr struct TypeEndl {} Endl;

template <LogLevel LEVEL>
class Logger {
public:
    Logger(const std::string log_file_path)
        : log_file_path_(log_file_path)
    {
        Open();
    }

    ~Logger() {
        Close();
    }

    template <typename T>
    Logger& operator<<(const T& data) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (!log_file_.is_open()) {
            return *this;
        }
        if (needs_prefix_) {
            WritePrefix();
        }
        log_file_ << data;
        return *this;
    }

    Logger& operator<<(const TypeEndl&) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (!log_file_.is_open()) {
            return *this;
        }
        log_file_ << std::endl;
        needs_prefix_ = true;
        return *this;
    }

private:
    void WritePrefix() {
        const auto now = std::chrono::system_clock::now();
        log_file_ << std::format("{:%Y-%m-%d %H:%M:%S}", now) << " ";

        if constexpr (LEVEL == LogLevel::DEBUG) {
            log_file_ << "[DEBUG] ";
        } else if constexpr (LEVEL == LogLevel::INFO) {
            log_file_ << "[INFO] ";
        } else if constexpr (LEVEL == LogLevel::WARNING) {
            log_file_ << "[WARNING] ";
        } else if constexpr (LEVEL == LogLevel::ERROR) {
            log_file_ << "[ERROR] ";
        }
        needs_prefix_ = false;
    }

    void Open() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_file_.open(log_file_path_, std::ios::app);
        needs_prefix_ = true;
    }

    void Close() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

private:
    std::mutex log_mutex_;
    std::ofstream log_file_;
    bool needs_prefix_ = true;
    const std::string log_file_path_;
};

template <LogLevel LEVEL>
class ProcessWatcher {
public:
    ProcessWatcher(Logger<LEVEL>& logger, const char* const process_name)
        : logger_(logger)
        , process_name_(process_name)
    {
        logger_ << process_name_ << " started!" << Endl;
    }

    ~ProcessWatcher() {
        logger_ << process_name_ << " aborted!" << Endl;
    }

private:
    Logger<LEVEL>& logger_;
    const char* const process_name_;
};

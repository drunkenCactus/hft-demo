#pragma once

#include <cstdint>
#include <chrono>
#include <ostream>

constexpr struct TypeEndl {} Endl;

namespace hft {

enum class LogLevel : uint8_t {
    kInfo,
    kWarning,
    kError,
};

std::ostream& operator<<(std::ostream& s, LogLevel level);

std::ostream& operator<<(std::ostream& s, const std::chrono::system_clock::time_point& time);

void WriteLog(std::ostream& s, const std::chrono::system_clock::time_point& time, LogLevel level, const char* message);

}  // namespace hft

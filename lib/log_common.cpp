#include "log_common.hpp"

namespace hft {

std::ostream& operator<<(std::ostream& s, LogLevel level) {
    switch (level) {
        case LogLevel::kInfo:
            s << "[INFO]";
            break;
        case LogLevel::kWarning:
            s << "[WARNING]";
            break;
        case LogLevel::kError:
            s << "[ERROR]";
            break;
    }
    return s;
}

std::ostream& operator<<(std::ostream& s, const std::chrono::system_clock::time_point& time) {
    s << std::format("{:%Y-%m-%d %H:%M:%S}", time);
    return s;
}

void WriteLog(std::ostream& s, const std::chrono::system_clock::time_point& time, LogLevel level, const char* message) {
    s << time << " "
      << level << " "
      << message << std::endl;
}

}  // namespace hft

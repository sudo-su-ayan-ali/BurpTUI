#include "util/Logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace BurpTUI {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setLevel(LogLevel lvl) {
    std::scoped_lock lk(mtx_);
    level_ = lvl;
}

void Logger::log(LogLevel lvl, std::string_view msg) {
    std::scoped_lock lk(mtx_);
    if (lvl < level_) return;
    const char* tag = "INFO";
    switch (lvl) {
        case LogLevel::DEBUG: tag = "DEBUG"; break;
        case LogLevel::WARN:  tag = "WARN";  break;
        case LogLevel::ERROR: tag = "ERROR"; break;
        default: break;
    }
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%H:%M:%S")
        << " [" << tag << "] " << msg << "\n";
    std::cerr << oss.str();
}

} // namespace BurpTUI

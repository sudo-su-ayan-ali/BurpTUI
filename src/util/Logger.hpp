#pragma once
#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <mutex>

namespace BurpTUI {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel lvl);
    void log(LogLevel lvl, std::string_view msg);

    void debug(std::string_view msg) { log(LogLevel::DEBUG, msg); }
    void info (std::string_view msg) { log(LogLevel::INFO,  msg); }
    void warn (std::string_view msg) { log(LogLevel::WARN,  msg); }
    void error(std::string_view msg) { log(LogLevel::ERROR, msg); }

private:
    Logger() = default;
    std::mutex  mtx_;
    LogLevel    level_ = LogLevel::INFO;
};

} // namespace BurpTUI

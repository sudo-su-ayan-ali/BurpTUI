#pragma once
#include <string>
#include <string_view>
#include <fstream>
#include <mutex>

namespace BurpTUI {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel lvl);
    void setLogFile(const std::string& path);
    void log(LogLevel lvl, std::string_view msg);

    void debug(std::string_view msg) { log(LogLevel::DEBUG, msg); }
    void info (std::string_view msg) { log(LogLevel::INFO,  msg); }
    void warn (std::string_view msg) { log(LogLevel::WARN,  msg); }
    void error(std::string_view msg) { log(LogLevel::ERROR, msg); }

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex    mtx_;
    LogLevel      level_ = LogLevel::DEBUG;
    std::ofstream fileStream_;
};

} // namespace BurpTUI

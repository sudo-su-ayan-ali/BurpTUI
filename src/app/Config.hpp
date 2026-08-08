#pragma once
#include <cstdint>
#include <string>

namespace BurpTUI {

struct Config {
    std::string  listenHost  = "127.0.0.1";
    std::uint16_t listenPort = 8080;
    std::string  dbPath      = "burptui.db";
    bool         verbose     = false;
};

} // namespace BurpTUI

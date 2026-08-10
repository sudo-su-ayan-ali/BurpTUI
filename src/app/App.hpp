#pragma once
#include "Config.hpp"
#include "util/TsQueue.hpp"
#include "http/HttpTransaction.hpp"
#include <memory>

namespace BurpTUI {

class ProxyServer;

class App {
public:
    explicit App(Config cfg = {});
    ~App();
    int run();

private:
    Config cfg_;
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue_;
    std::unique_ptr<ProxyServer> proxy_;
};

} // namespace BurpTUI

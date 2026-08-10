#include "App.hpp"
#include "proxy/ProxyServer.hpp"
#include "tui/TuiApp.hpp"
#include "util/Logger.hpp"

namespace BurpTUI {

App::App(Config cfg)
    : cfg_(std::move(cfg))
    , txQueue_(std::make_shared<TsQueue<HttpTransaction>>())
{}

App::~App() {
    if (proxy_) proxy_->stop();
}

int App::run() {
    auto queue = txQueue_;
    proxy_ = std::make_unique<ProxyServer>(
        cfg_.listenHost, cfg_.listenPort,
        [queue](HttpTransaction tx) {
            queue->push(std::move(tx));
        });

    proxy_->start();
    Logger::instance().info("Proxy started on " + cfg_.listenHost + ":" + std::to_string(cfg_.listenPort));

    TuiApp tui(cfg_, txQueue_);
    tui.run();

    proxy_->stop();
    Logger::instance().info("Proxy stopped.");
    return 0;
}

} // namespace BurpTUI

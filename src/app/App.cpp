#include "App.hpp"
#include "proxy/ProxyServer.hpp"
#include "proxy/InterceptManager.hpp"
#include "tui/TuiApp.hpp"
#include "util/Logger.hpp"
#include "proxy/SslInit.hpp"

namespace BurpTUI {

App::App(Config cfg)
    : cfg_(std::move(cfg))
    , txQueue_(std::make_shared<TsQueue<HttpTransaction>>())
{}

App::~App() {
    if (proxy_) proxy_->stop();
}

int App::run() {
    if (!SslInit::instance().initialize(cfg_.caDir)) {
        Logger::instance().error("Fatal: OpenSSL initialization failed");
        return 1;
    }
    Logger::instance().info("CA Certificate available at: " + cfg_.caDir + "/ca.crt");

    TuiApp tui(cfg_, txQueue_);
    auto trigger = tui.getUpdateTrigger();
    InterceptManager::instance().setNotifyCallback(trigger);
    auto queue = txQueue_;
    
    proxy_ = std::make_unique<ProxyServer>(
        cfg_.listenHost, cfg_.listenPort,
        [queue, trigger](HttpTransaction tx) {
            queue->push(std::move(tx));
            if (trigger) trigger();
        });

    proxy_->start();
    Logger::instance().info("Proxy started on " + cfg_.listenHost + ":" + std::to_string(cfg_.listenPort));

    tui.run();

    proxy_->stop();
    Logger::instance().info("Proxy stopped.");
    return 0;
}

} // namespace BurpTUI

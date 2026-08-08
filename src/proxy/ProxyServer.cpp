#include "proxy/ProxyServer.hpp"

namespace BurpTUI {

struct ProxyServer::Impl {
    std::string   host;
    std::uint16_t port;
    bool          running = false;
};

ProxyServer::ProxyServer(const std::string& host,
                         std::uint16_t       port,
                         EventCallback       /*onEvent*/)
    : impl_(std::make_unique<Impl>(host, port))
{}

ProxyServer::~ProxyServer() { stop(); }

void ProxyServer::start() { impl_->running = true;  /* TODO: Phase 2 */ }
void ProxyServer::stop()  { impl_->running = false; /* TODO */ }
bool ProxyServer::isRunning() const { return impl_->running; }

} // namespace BurpTUI

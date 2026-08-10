#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include "proxy/Session.hpp"  // for TransactionCallback

namespace BurpTUI {

class ProxyServer {
public:
    explicit ProxyServer(const std::string& host,
                         std::uint16_t port,
                         TransactionCallback onTransaction = {});
    ~ProxyServer();

    void start();  ///< Non-blocking; launches io_context on a background thread
    void stop();   ///< Signals io_context to stop and joins the thread
    bool isRunning() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace BurpTUI

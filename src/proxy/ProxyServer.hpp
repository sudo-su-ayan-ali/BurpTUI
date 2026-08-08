#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <functional>

namespace BurpTUI {

/// Listens on the configured port and spawns Sessions/MitmSessions.
class ProxyServer {
public:
    using EventCallback = std::function<void(const std::string& event)>;

    explicit ProxyServer(const std::string& host,
                         std::uint16_t       port,
                         EventCallback       onEvent = {});
    ~ProxyServer();

    void start();  ///< Non-blocking; runs the io_context in a thread pool.
    void stop();

    bool isRunning() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace BurpTUI

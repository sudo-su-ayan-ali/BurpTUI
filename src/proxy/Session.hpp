#pragma once
#include <memory>
#include <string>
#include <cstdint>

namespace BurpTUI {

/// Opaque forward declaration — implementation uses Boost.Asio.
class Session {
public:
    explicit Session(std::uint64_t id, const std::string& remoteIp);
    ~Session();

    std::uint64_t id() const;
    const std::string& remoteIp() const;

    void start();
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace BurpTUI

#pragma once
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
    Impl* impl_ = nullptr;
};

} // namespace BurpTUI

#pragma once
#include <string>
#include <cstdint>

namespace BurpTUI {

/// HTTPS MITM session: terminates TLS on both sides.
class MitmSession {
public:
    explicit MitmSession(std::uint64_t id,
                         const std::string& remoteIp,
                         const std::string& targetHost,
                         std::uint16_t      targetPort);
    ~MitmSession();

    void start();
    void close();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace BurpTUI

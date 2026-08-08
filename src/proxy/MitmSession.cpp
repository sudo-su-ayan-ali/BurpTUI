#include "proxy/MitmSession.hpp"

namespace BurpTUI {

struct MitmSession::Impl {
    std::uint64_t id;
    std::string   remoteIp;
    std::string   targetHost;
    std::uint16_t targetPort;
};

MitmSession::MitmSession(std::uint64_t      id,
                         const std::string& remoteIp,
                         const std::string& targetHost,
                         std::uint16_t      targetPort)
    : impl_(new Impl{id, remoteIp, targetHost, targetPort})
{}

MitmSession::~MitmSession() { delete impl_; }

void MitmSession::start() { /* TODO: Phase 2 — TLS MITM */ }
void MitmSession::close() { /* TODO */ }

} // namespace BurpTUI

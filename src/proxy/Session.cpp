#include "proxy/Session.hpp"

namespace BurpTUI {

struct Session::Impl {
    std::uint64_t id;
    std::string   remoteIp;
};

Session::Session(std::uint64_t id, const std::string& remoteIp)
    : impl_(std::make_unique<Impl>(id, remoteIp))
{}
Session::~Session() { }

std::uint64_t Session::id()       const { return impl_->id; }
const std::string& Session::remoteIp() const { return impl_->remoteIp; }

void Session::start() { /* TODO: Boost.Asio async_read chain */ }
void Session::close() { /* TODO */ }

} // namespace BurpTUI

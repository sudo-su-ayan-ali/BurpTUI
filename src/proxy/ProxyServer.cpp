#include "proxy/ProxyServer.hpp"
#include "proxy/Session.hpp"
#include "util/Logger.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <atomic>

namespace BurpTUI {

struct ProxyServer::Impl {
    std::string host_;
    std::uint16_t port_;
    TransactionCallback onTransaction_;
    
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<int> nextId_{1};

    Impl(const std::string& host, std::uint16_t port, TransactionCallback cb)
        : host_(host), port_(port), onTransaction_(std::move(cb)),
          acceptor_(io_context_), work_guard_(boost::asio::make_work_guard(io_context_)) {}

    void doAccept() {
        if (!running_) return;
        Logger::instance().debug("doAccept() called");
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                Logger::instance().debug("async_accept callback fired");
                if (!ec) {
                    auto session = std::make_shared<Session>(std::move(socket), onTransaction_, nextId_);
                    session->start();
                } else if (running_) {
                    Logger::instance().error("Accept error: " + ec.message());
                }
                doAccept();
            });
    }
};

ProxyServer::ProxyServer(const std::string& host, std::uint16_t port, TransactionCallback onTransaction)
    : impl_(std::make_unique<Impl>(host, port, std::move(onTransaction))) {
}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    if (impl_->running_) return;
    
    try {
        boost::asio::ip::tcp::resolver resolver(impl_->io_context_);
        auto endpoints = resolver.resolve(impl_->host_, std::to_string(impl_->port_));
        
        impl_->acceptor_.open(endpoints.begin()->endpoint().protocol());
        impl_->acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        impl_->acceptor_.bind(endpoints.begin()->endpoint());
        impl_->acceptor_.listen();

        impl_->running_ = true;
        impl_->doAccept();

        impl_->thread_ = std::thread([this]() {
            Logger::instance().info("ProxyServer started on " + impl_->host_ + ":" + std::to_string(impl_->port_));
            try {
                impl_->io_context_.run();
            } catch (const std::exception& e) {
                Logger::instance().error(std::string("io_context.run exception: ") + e.what());
            }
            Logger::instance().info("ProxyServer io_context loop exited");
        });
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Failed to start ProxyServer: ") + e.what());
        impl_->running_ = false;
    }
}

void ProxyServer::stop() {
    if (!impl_->running_) return;
    
    Logger::instance().info("Stopping ProxyServer...");
    impl_->running_ = false;
    impl_->acceptor_.close();
    impl_->work_guard_.reset();
    impl_->io_context_.stop();
    
    if (impl_->thread_.joinable()) {
        impl_->thread_.join();
    }
    Logger::instance().info("ProxyServer stopped.");
}

bool ProxyServer::isRunning() const {
    return impl_->running_;
}

} // namespace BurpTUI

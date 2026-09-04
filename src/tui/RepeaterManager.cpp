#include "tui/RepeaterManager.hpp"
#include "util/Logger.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <chrono>

namespace BurpTUI {

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

RepeaterManager& RepeaterManager::instance() {
    static RepeaterManager inst;
    return inst;
}

RepeaterManager::RepeaterManager() = default;

void RepeaterManager::setTarget(std::string host, std::string port, bool isHttps) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_ = std::move(host);
    port_ = std::move(port);
    isHttps_ = isHttps;
    notifyCallback_ ? notifyCallback_() : void();
}

void RepeaterManager::setRequest(std::string host, std::string port, bool isHttps, std::string rawRequest) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = std::move(host);
        port_ = std::move(port);
        isHttps_ = isHttps;
        rawRequest_ = std::move(rawRequest);
        statusText_ = "Loaded request for " + host_ + ":" + port_;
    }
    if (notifyCallback_) notifyCallback_();
    switchToRepeaterTab();
}

void RepeaterManager::setRawRequest(std::string raw) {
    std::lock_guard<std::mutex> lock(mutex_);
    rawRequest_ = std::move(raw);
}

void RepeaterManager::setRawResponse(std::string raw) {
    std::lock_guard<std::mutex> lock(mutex_);
    rawResponse_ = std::move(raw);
}

std::string RepeaterManager::getHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return host_;
}

std::string RepeaterManager::getPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

bool RepeaterManager::isHttps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isHttps_;
}

std::string RepeaterManager::getRawRequest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rawRequest_;
}

std::string RepeaterManager::getRawResponse() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rawResponse_;
}

std::string RepeaterManager::getStatusText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statusText_;
}

bool RepeaterManager::isSending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isSending_;
}

void RepeaterManager::setNotifyCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    notifyCallback_ = std::move(callback);
}

void RepeaterManager::setSwitchTabCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    switchTabCallback_ = std::move(callback);
}

void RepeaterManager::switchToRepeaterTab() {
    std::function<void(int)> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = switchTabCallback_;
    }
    if (cb) {
        cb(2); // Repeater is tab index 2
    }
}

void RepeaterManager::executeRequest() {
    std::string host;
    std::string port;
    bool isHttps;
    std::string req;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isSending_) return;
        isSending_ = true;
        host = host_;
        port = port_;
        isHttps = isHttps_;
        req = rawRequest_;
    }

    // Sanitize host and scheme
    while (!host.empty() && (host.front() == ' ' || host.front() == '\t')) host.erase(0, 1);
    while (!host.empty() && (host.back() == ' ' || host.back() == '\t')) host.pop_back();

    if (host.starts_with("https://")) {
        host = host.substr(8);
        isHttps = true;
    } else if (host.starts_with("http://")) {
        host = host.substr(7);
        isHttps = false;
    }

    auto slashPos = host.find('/');
    if (slashPos != std::string::npos) {
        host = host.substr(0, slashPos);
    }

    auto colonPos = host.rfind(':');
    if (colonPos != std::string::npos && host.find(']') == std::string::npos) {
        port = host.substr(colonPos + 1);
        host = host.substr(0, colonPos);
    }

    if (port.empty()) {
        port = isHttps ? "443" : "80";
    }

    // Clean and normalize HTTP request
    // Remove accidental (HTTPS) or (HTTP) in Host line
    auto p1 = req.find(" (HTTPS)");
    if (p1 != std::string::npos) req.erase(p1, 8);
    auto p2 = req.find(" (HTTP)");
    if (p2 != std::string::npos) req.erase(p2, 7);

    // Normalize CRLF
    std::string cleanReq;
    cleanReq.reserve(req.size() + 64);
    for (char c : req) {
        if (c == '\r') continue;
        if (c == '\n') cleanReq += "\r\n";
        else cleanReq += c;
    }

    // Ensure headers terminate with double CRLF
    if (cleanReq.find("\r\n\r\n") == std::string::npos) {
        if (cleanReq.ends_with("\r\n")) {
            cleanReq += "\r\n";
        } else {
            cleanReq += "\r\n\r\n";
        }
    }

    // Ensure Host header is present
    if (cleanReq.find("Host:") == std::string::npos && cleanReq.find("host:") == std::string::npos) {
        auto firstLineEnd = cleanReq.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            cleanReq.insert(firstLineEnd + 2, "Host: " + host + "\r\n");
        }
    }

    // Ensure Connection: close is set so socket reads terminate promptly
    if (cleanReq.find("Connection:") == std::string::npos && cleanReq.find("connection:") == std::string::npos) {
        auto firstLineEnd = cleanReq.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            cleanReq.insert(firstLineEnd + 2, "Connection: close\r\n");
        }
    }

    req = std::move(cleanReq);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = host;
        port_ = port;
        isHttps_ = isHttps;
        statusText_ = "Connecting to " + host + ":" + port + "...";
    }
    if (notifyCallback_) notifyCallback_();

    std::thread([this, host, port, isHttps, req]() {
        auto startTime = std::chrono::steady_clock::now();
        std::string response;
        std::string errStr;

        try {
            boost::asio::io_context ioc;
            tcp::resolver resolver(ioc);
            auto endpoints = resolver.resolve(host, port);

            if (!isHttps) {
                tcp::socket socket(ioc);
                boost::asio::connect(socket, endpoints);
                boost::asio::write(socket, boost::asio::buffer(req));

                char buf[8192];
                boost::system::error_code ec;
                while (true) {
                    size_t len = socket.read_some(boost::asio::buffer(buf), ec);
                    if (ec || len == 0) break;
                    response.append(buf, len);
                }
                boost::system::error_code ignored;
                socket.shutdown(tcp::socket::shutdown_both, ignored);
            } else {
                ssl::context ctx(ssl::context::tlsv12_client);
                ctx.set_default_verify_paths();
                ctx.set_verify_mode(ssl::verify_none);

                ssl::stream<tcp::socket> stream(ioc, ctx);
                SSL_set_tlsext_host_name(stream.native_handle(), host.c_str());

                boost::asio::connect(stream.next_layer(), endpoints);
                stream.handshake(ssl::stream_base::client);
                boost::asio::write(stream, boost::asio::buffer(req));

                char buf[8192];
                boost::system::error_code ec;
                while (true) {
                    size_t len = stream.read_some(boost::asio::buffer(buf), ec);
                    if (ec || len == 0) break;
                    response.append(buf, len);
                }
                boost::system::error_code ignored;
                stream.shutdown(ignored);
            }
        } catch (const std::exception& ex) {
            errStr = ex.what();
        }

        auto endTime = std::chrono::steady_clock::now();
        long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            isSending_ = false;
            if (!errStr.empty()) {
                statusText_ = "Error: " + errStr;
                rawResponse_ = "HTTP Connection Failed: " + errStr + "\nTarget: " + (isHttps ? "https://" : "http://") + host + ":" + port;
            } else {
                statusText_ = "Done (" + std::to_string(response.size()) + " B in " + std::to_string(elapsedMs) + " ms)";
                rawResponse_ = std::move(response);
            }
        }
        if (notifyCallback_) notifyCallback_();
    }).detach();
}

} // namespace BurpTUI

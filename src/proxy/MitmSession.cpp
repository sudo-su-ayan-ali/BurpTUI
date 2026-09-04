#include "proxy/MitmSession.hpp"
#include "proxy/InterceptManager.hpp"
#include "util/Logger.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace BurpTUI {

MitmSession::MitmSession(boost::asio::ip::tcp::socket clientSocket,
                         std::string targetHost,
                         std::uint16_t targetPort,
                         TransactionCallback onTransaction,
                         std::atomic<int>& nextId,
                         std::shared_ptr<CertCache> certCache)
    : rawClientSocket_(std::move(clientSocket)),
      rawServerSocket_(rawClientSocket_.get_executor()),
      resolver_(rawClientSocket_.get_executor()),
      timer_(rawClientSocket_.get_executor()),
      targetHost_(std::move(targetHost)),
      targetPort_(targetPort),
      onTransaction_(std::move(onTransaction)),
      nextId_(nextId),
      certCache_(std::move(certCache))
{
}

MitmSession::~MitmSession() {
    close();
}

void MitmSession::start() {
    Logger::instance().debug("MitmSession: Starting for target " + targetHost_ + ":" + std::to_string(targetPort_));
    resetTimer();
    connectUpstream();
}

void MitmSession::close() {
    if (isClosed_) return;
    isClosed_ = true;

    if (currentTransaction_.id != 0) {
        InterceptManager::instance().removeById(currentTransaction_.id);
    }

    boost::system::error_code ec;
    timer_.cancel(ec);

    if (clientStream_) {
        clientStream_->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        clientStream_->lowest_layer().close(ec);
    } else if (rawClientSocket_.is_open()) {
        rawClientSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        rawClientSocket_.close(ec);
    }

    if (upstreamStream_) {
        upstreamStream_->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        upstreamStream_->lowest_layer().close(ec);
    } else if (rawServerSocket_.is_open()) {
        rawServerSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        rawServerSocket_.close(ec);
    }
}

void MitmSession::resetTimer() {
    timer_.expires_after(std::chrono::seconds(30));
    auto self = shared_from_this();
    timer_.async_wait([this, self](boost::system::error_code ec) {
        handleTimeout(ec);
    });
}

void MitmSession::handleTimeout(boost::system::error_code ec) {
    if (ec != boost::asio::error::operation_aborted) {
        Logger::instance().debug("MitmSession: Timeout — closing connection for " + targetHost_);
        close();
    }
}

void MitmSession::connectUpstream() {
    auto self = shared_from_this();
    Logger::instance().debug("MitmSession: Resolving DNS for " + targetHost_ + ":" + std::to_string(targetPort_));
    resolver_.async_resolve(
        targetHost_, std::to_string(targetPort_),
        [this, self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
            if (isClosed_) return;
            if (ec) {
                Logger::instance().error("MitmSession: DNS resolution failed for " + targetHost_ + ": " + ec.message());
                sendErrorResponse(502, "Bad Gateway");
                return;
            }
            handleUpstreamConnect(ec, results);
        });
}

void MitmSession::handleUpstreamConnect(boost::system::error_code /*ec*/,
                                       boost::asio::ip::tcp::resolver::results_type results) {
    if (isClosed_) return;
    auto self = shared_from_this();
    Logger::instance().debug("MitmSession: Connecting to upstream " + targetHost_ + ":" + std::to_string(targetPort_));
    boost::asio::async_connect(
        rawServerSocket_, results,
        [this, self](boost::system::error_code ec, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
            if (isClosed_) return;
            if (ec) {
                Logger::instance().error("MitmSession: Upstream connect failed for " + targetHost_ + ": " + ec.message());
                sendErrorResponse(502, "Bad Gateway");
                return;
            }
            sendEstablishedResponse();
        });
}

void MitmSession::sendEstablishedResponse() {
    if (isClosed_) return;
    auto self = shared_from_this();
    static const std::string established = "HTTP/1.1 200 Connection Established\r\n\r\n";
    boost::asio::async_write(
        rawClientSocket_, boost::asio::buffer(established),
        [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            if (isClosed_) return;
            if (ec) {
                Logger::instance().error("MitmSession: Failed to write 200 Connection Established: " + ec.message());
                close();
                return;
            }
            startTlsHandshakes();
        });
}

void MitmSession::sendErrorResponse(int statusCode, const std::string& statusText) {
    if (isClosed_) return;
    std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
    closeAfterWrite_ = true;

    currentTransaction_.response = std::make_shared<HttpResponse>();
    currentTransaction_.response->version = "HTTP/1.1";
    currentTransaction_.response->statusCode = statusCode;
    currentTransaction_.response->statusText = statusText;
    if (onTransaction_) {
        onTransaction_(currentTransaction_);
    }

    if (clientHandshakeDone_ && clientStream_) {
        writeClient(std::move(response));
    } else {
        auto self = shared_from_this();
        boost::asio::async_write(
            rawClientSocket_, boost::asio::buffer(response),
            [this, self](boost::system::error_code /*ec*/, std::size_t /*bytes_transferred*/) {
                close();
            });
    }
}

void MitmSession::startTlsHandshakes() {
    if (isClosed_) return;
    if (!certCache_) {
        Logger::instance().error("MitmSession: No CertCache configured");
        close();
        return;
    }

    clientSslContext_ = certCache_->getContext(targetHost_);
    if (!clientSslContext_) {
        Logger::instance().error("MitmSession: Failed to obtain SSL context for " + targetHost_);
        close();
        return;
    }

    // Wrap client raw socket in server SSL stream
    clientStream_ = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
        std::move(rawClientSocket_), *clientSslContext_);

    // Configure upstream client SSL context
    upstreamSslContext_.set_default_verify_paths();
    upstreamSslContext_.set_verify_mode(boost::asio::ssl::verify_none);

    // Wrap upstream raw socket in client SSL stream
    upstreamStream_ = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
        std::move(rawServerSocket_), upstreamSslContext_);

    // Set SNI only if targetHost_ is a domain name, not an IP address (RFC 6066)
    boost::system::error_code ipEc;
    boost::asio::ip::make_address(targetHost_, ipEc);
    if (ipEc) {
        SSL_set_tlsext_host_name(upstreamStream_->native_handle(), targetHost_.c_str());
    }

    // Configure ALPN upstream: advertise http/1.1
    static const unsigned char alpnProtos[] = "\x08http/1.1";
    SSL_set_alpn_protos(upstreamStream_->native_handle(), alpnProtos, sizeof(alpnProtos) - 1);

    auto self = shared_from_this();

    clientStream_->async_handshake(
        boost::asio::ssl::stream_base::server,
        [this, self](boost::system::error_code ec) {
            handleClientHandshake(ec);
        });

    upstreamStream_->async_handshake(
        boost::asio::ssl::stream_base::client,
        [this, self](boost::system::error_code ec) {
            handleUpstreamHandshake(ec);
        });
}

void MitmSession::handleClientHandshake(boost::system::error_code ec) {
    if (isClosed_) return;
    if (ec) {
        Logger::instance().error("MitmSession: Client TLS handshake failed for " + targetHost_ + ": " + ec.message());
        close();
        return;
    }
    Logger::instance().debug("MitmSession: Client TLS handshake completed for " + targetHost_);
    clientHandshakeDone_ = true;
    if (upstreamHandshakeDone_) {
        onBothHandshakesComplete();
    }
}

void MitmSession::handleUpstreamHandshake(boost::system::error_code ec) {
    if (isClosed_) return;
    if (ec) {
        Logger::instance().error("MitmSession: Upstream TLS handshake failed for " + targetHost_ + ": " + ec.message());
        close();
        return;
    }
    Logger::instance().debug("MitmSession: Upstream TLS handshake completed for " + targetHost_);
    upstreamHandshakeDone_ = true;
    if (clientHandshakeDone_) {
        onBothHandshakesComplete();
    }
}

void MitmSession::onBothHandshakesComplete() {
    if (isClosed_) return;
    Logger::instance().info("MitmSession: TLS handshakes established with both client and " + targetHost_);
    readClient();
}

void MitmSession::readClient() {
    if (isClosed_) return;
    auto self = shared_from_this();
    clientStream_->async_read_some(
        boost::asio::buffer(clientBuffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleClientRead(ec, bytes_transferred);
        });
}

void MitmSession::handleClientRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (isClosed_) return;
    if (ec) {
        if (ec != boost::asio::error::eof &&
            ec != boost::asio::error::operation_aborted &&
            ec != boost::asio::ssl::error::stream_truncated) {
            Logger::instance().error("MitmSession: Client read error: " + ec.message());
        }
        close();
        return;
    }

    resetTimer();
    std::string_view data(clientBuffer_.data(), bytes_transferred);
    clientAccumulated_.append(data);

    if (clientParser_.feedRequest(data)) {
        if (auto req = clientParser_.takeRequest()) {
            currentTransaction_.id = nextId_++;
            currentTransaction_.host = targetHost_;
            currentTransaction_.port = targetPort_;
            currentTransaction_.is_https = true;
            currentTransaction_.request = std::make_shared<HttpRequest>(*req);

            if (InterceptManager::instance().isInterceptEnabled()) {
                boost::system::error_code ignored;
                timer_.cancel(ignored);
                auto self = shared_from_this();
                InterceptManager::instance().interceptRequest(
                    currentTransaction_.id,
                    /*isHttps=*/true,
                    targetHost_,
                    targetPort_,
                    currentTransaction_.request,
                    [this, self](InterceptAction action) {
                        boost::asio::post(rawClientSocket_.get_executor(), [this, self, action]() {
                            if (isClosed_) return;
                            resetTimer();
                            if (action == InterceptAction::Forward) {
                                Logger::instance().debug("MitmSession: Forwarding HTTPS " + currentTransaction_.request->method + " " + currentTransaction_.request->url);
                                std::string serialized = currentTransaction_.request->serialize();
                                writeUpstream(std::move(serialized));
                            } else {
                                Logger::instance().info("MitmSession: Dropping HTTPS request #" + std::to_string(currentTransaction_.id));
                                sendErrorResponse(502, "Request Dropped by BurpTUI");
                            }
                        });
                    }
                );
                return;
            }

            Logger::instance().debug("MitmSession: Forwarding HTTPS " + req->method + " " + req->url);

            std::string serialized = req->serialize();
            writeUpstream(std::move(serialized));
            return;
        } else {
            readClient();
        }
    } else {
        if (clientAccumulated_.size() < 1048576) {
            readClient();
        } else {
            Logger::instance().error("MitmSession: Request too large or unparseable");
            close();
        }
    }
}

void MitmSession::writeUpstream(std::string data) {
    if (isClosed_) return;
    bool idle = upstreamWriteQueue_.empty();
    upstreamWriteQueue_.push_back(std::move(data));
    if (idle) {
        doUpstreamWrite();
    }
}

void MitmSession::doUpstreamWrite() {
    if (isClosed_ || upstreamWriteQueue_.empty()) return;
    auto self = shared_from_this();
    boost::asio::async_write(
        *upstreamStream_,
        boost::asio::buffer(upstreamWriteQueue_.front()),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleUpstreamWrite(ec, bytes_transferred);
        });
}

void MitmSession::handleUpstreamWrite(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (isClosed_) return;
    if (ec) {
        Logger::instance().error("MitmSession: Upstream write failed: " + ec.message());
        close();
        return;
    }
    upstreamWriteQueue_.pop_front();
    if (!upstreamWriteQueue_.empty()) {
        doUpstreamWrite();
    } else {
        readUpstream();
    }
}

void MitmSession::readUpstream() {
    if (isClosed_) return;
    auto self = shared_from_this();
    upstreamStream_->async_read_some(
        boost::asio::buffer(upstreamBuffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleUpstreamRead(ec, bytes_transferred);
        });
}

void MitmSession::handleUpstreamRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (isClosed_) return;
    if (ec) {
        if (ec == boost::asio::error::eof || ec == boost::asio::ssl::error::stream_truncated) {
            if (!currentTransaction_.response && !upstreamAccumulated_.empty()) {
                upstreamParser_.feedResponse(upstreamAccumulated_);
                if (auto res = upstreamParser_.takeResponse()) {
                    currentTransaction_.response = std::make_shared<HttpResponse>(*res);
                    if (onTransaction_) {
                        onTransaction_(currentTransaction_);
                    }
                }
            }
        } else if (ec != boost::asio::error::operation_aborted) {
            Logger::instance().error("MitmSession: Upstream read error: " + ec.message());
        }
        close();
        return;
    }

    resetTimer();
    std::string_view data(upstreamBuffer_.data(), bytes_transferred);
    std::string rawChunk(data);
    writeClient(rawChunk);

    upstreamAccumulated_.append(data);

    if (upstreamParser_.feedResponse(data)) {
        if (auto res = upstreamParser_.takeResponse()) {
            currentTransaction_.response = std::make_shared<HttpResponse>(*res);
            if (onTransaction_) {
                onTransaction_(currentTransaction_);
            }

            // Check keep-alive
            std::string connReq = currentTransaction_.request ? currentTransaction_.request->header("Connection") : "";
            std::string connRes = res->header("Connection");
            bool keepAlive = false;

            if (currentTransaction_.request && currentTransaction_.request->version == "HTTP/1.1") {
                keepAlive = (connReq.find("close") == std::string::npos &&
                             connRes.find("close") == std::string::npos);
            } else {
                keepAlive = (connReq.find("keep-alive") != std::string::npos ||
                             connRes.find("keep-alive") != std::string::npos);
            }

            if (keepAlive) {
                clientParser_.reset();
                upstreamParser_.reset();
                clientAccumulated_.clear();
                upstreamAccumulated_.clear();
                currentTransaction_ = HttpTransaction{};
                readClient();
            } else {
                closeAfterWrite_ = true;
                if (clientWriteQueue_.empty()) {
                    close();
                }
            }
        } else {
            readUpstream();
        }
    } else {
        readUpstream();
    }
}

void MitmSession::writeClient(std::string data) {
    if (isClosed_) return;
    bool idle = clientWriteQueue_.empty();
    clientWriteQueue_.push_back(std::move(data));
    if (idle) {
        doClientWrite();
    }
}

void MitmSession::doClientWrite() {
    if (isClosed_ || clientWriteQueue_.empty()) return;
    auto self = shared_from_this();
    boost::asio::async_write(
        *clientStream_,
        boost::asio::buffer(clientWriteQueue_.front()),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleClientWrite(ec, bytes_transferred);
        });
}

void MitmSession::handleClientWrite(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (isClosed_) return;
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            Logger::instance().error("MitmSession: Client write failed: " + ec.message());
        }
        close();
        return;
    }
    clientWriteQueue_.pop_front();
    if (!clientWriteQueue_.empty()) {
        doClientWrite();
        return;
    }
    if (closeAfterWrite_) {
        close();
    }
}

} // namespace BurpTUI

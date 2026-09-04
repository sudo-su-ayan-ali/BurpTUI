#include "proxy/Session.hpp"
#include "proxy/MitmSession.hpp"
#include "proxy/SslInit.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "util/Logger.hpp"
#include <iostream>
#include <fstream>

namespace BurpTUI {

Session::Session(boost::asio::ip::tcp::socket clientSocket,
                 TransactionCallback onTransaction,
                 std::atomic<int>& nextId,
                 std::shared_ptr<CertCache> certCache)
    : clientSocket_(std::move(clientSocket)),
      serverSocket_(clientSocket_.get_executor()),
      resolver_(clientSocket_.get_executor()),
      timer_(clientSocket_.get_executor()),
      onTransaction_(std::move(onTransaction)),
      nextId_(nextId),
      certCache_(std::move(certCache)) {
}

Session::~Session() {
    close();
}

void Session::start() {
    Logger::instance().debug("Session started for client: " + clientSocket_.remote_endpoint().address().to_string());
    resetTimer();
    readClient();
}

void Session::close() {
    boost::system::error_code ec;
    timer_.cancel(ec);
    if (clientSocket_.is_open()) {
        clientSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        clientSocket_.close(ec);
    }
    if (serverSocket_.is_open()) {
        serverSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        serverSocket_.close(ec);
    }
}

void Session::resetTimer() {
    timer_.expires_after(std::chrono::seconds(30));
    auto self = shared_from_this();
    timer_.async_wait([this, self](boost::system::error_code ec) {
        handleTimeout(ec);
    });
}

void Session::handleTimeout(boost::system::error_code ec) {
    if (ec != boost::asio::error::operation_aborted) {
        if (currentTransaction_.request && !currentTransaction_.response && !isConnect_) {
            Logger::instance().debug("Session timeout — waiting for upstream, sending 504 Gateway Timeout");
            boost::system::error_code ignored;
            resolver_.cancel();
            if (serverSocket_.is_open()) {
                serverSocket_.cancel(ignored);
            }
            sendErrorResponse(504, "Gateway Timeout");
        } else {
            Logger::instance().debug("Session timeout — closing idle connection");
            close();
        }
    }
}

void Session::readClient() {
    auto self = shared_from_this();
    clientSocket_.async_read_some(
        boost::asio::buffer(clientBuffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleClientRead(ec, bytes_transferred);
        });
}

void Session::handleClientRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec != boost::asio::error::eof && ec != boost::asio::error::operation_aborted) {
            Logger::instance().error("Client read error: " + ec.message());
        }
        close();
        return;
    }

    resetTimer();
    std::string_view data(clientBuffer_.data(), bytes_transferred);
    
    if (isConnect_) {
        // Blind tunnel — forward everything from client directly to server
        writeServerBlind(std::string(data));
        readClient();
        return;
    }

    clientData_.append(data);

    if (parser_.feedRequest(data)) {
        if (auto req = parser_.takeRequest()) {
            currentTransaction_.id = nextId_++;
            currentTransaction_.request = std::make_shared<HttpRequest>(*req);
            
            // Handle CONNECT method (HTTPS tunneling — Phase 3 full MITM)
            if (req->method == "CONNECT") {
                // Parse host:port from URL (CONNECT host:port HTTP/1.1)
                std::string hostPort = req->url;
                std::string host = hostPort;
                std::string port = "443";
                auto colon = hostPort.find(':');
                if (colon != std::string::npos) {
                    host = hostPort.substr(0, colon);
                    port = hostPort.substr(colon + 1);
                }
                currentTransaction_.host = host;
                currentTransaction_.port = std::stoi(port.empty() ? "443" : port);
                currentTransaction_.is_https = true;
                
                // Respond with 200 and log the CONNECT tunnel
                currentTransaction_.response = std::make_shared<HttpResponse>();
                currentTransaction_.response->version = "HTTP/1.1";
                currentTransaction_.response->statusCode = 200;
                currentTransaction_.response->statusText = "Connection Established";
                if (onTransaction_) {
                    onTransaction_(currentTransaction_);
                }

                if (certCache_) {
                    boost::system::error_code ignored;
                    timer_.cancel(ignored);
                    auto mitmSession = std::make_shared<MitmSession>(
                        std::move(clientSocket_),
                        host,
                        static_cast<std::uint16_t>(std::stoi(port.empty() ? "443" : port)),
                        onTransaction_,
                        nextId_,
                        certCache_
                    );
                    mitmSession->start();
                    return;
                }

                // Fallback to blind tunnel if no certCache_ available
                isConnect_ = true;
                upstreamHost_ = host;
                upstreamPort_ = port;
                Logger::instance().info("CONNECT tunnel fallback (blind) for: " + host + ":" + port);
                connectUpstream();
                return;
            }

            // Extract target host from Host header or absolute URL
            std::string hostPort = req->header("Host");
            if (hostPort.empty()) {
                // Try parsing absolute URL (e.g., http://example.com/path)
                auto pos = req->url.find("://");
                if (pos != std::string::npos) {
                    auto hostStart = pos + 3;
                    auto hostEnd = req->url.find('/', hostStart);
                    if (hostEnd != std::string::npos) {
                        hostPort = req->url.substr(hostStart, hostEnd - hostStart);
                    } else {
                        hostPort = req->url.substr(hostStart);
                    }
                }
            }

            if (hostPort.empty()) {
                Logger::instance().error("No Host header and no absolute URL — cannot route request");
                sendErrorResponse(400, "Bad Request");
                return;
            }

            std::string host = hostPort;
            std::string port = "80";
            auto colon = hostPort.find(':');
            if (colon != std::string::npos) {
                host = hostPort.substr(0, colon);
                port = hostPort.substr(colon + 1);
            }

            // Check for Burp-style CA certificate download endpoints:
            // e.g. http://burp, http://burptui, http://burp/cert, /cert, or http://127.0.0.1:8080/cert
            std::string hostLower = host;
            for (auto& c : hostLower) c = static_cast<char>(std::tolower(c));
            if (hostLower == "burp" || hostLower == "burptui" ||
                req->url == "/cert" || req->url.ends_with("/cert") ||
                ((hostLower == "127.0.0.1" || hostLower == "localhost") && (req->url == "/cert" || req->url.find("/cert") != std::string::npos))) {
                if (req->url == "/cert" || req->url.ends_with("/cert") || req->url.find("/cert") != std::string::npos) {
                    serveCaCert();
                } else {
                    serveBurpHelpPage();
                }
                return;
            }

            currentTransaction_.host = host;
            currentTransaction_.port = std::stoi(port.empty() ? "80" : port);
            currentTransaction_.is_https = false;
            
            std::string prevHost = upstreamHost_;
            std::string prevPort = upstreamPort_;
            
            upstreamHost_ = host;
            upstreamPort_ = port.empty() ? "80" : port;
            
            if (serverSocket_.is_open() && upstreamHost_ == prevHost && upstreamPort_ == prevPort) {
                Logger::instance().debug("Reusing upstream connection for: " + upstreamHost_ + ":" + upstreamPort_);
                writeUpstream();
            } else {
                if (serverSocket_.is_open()) {
                    boost::system::error_code ec2;
                    serverSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec2);
                    serverSocket_.close(ec2);
                }
                serverSocket_ = boost::asio::ip::tcp::socket(clientSocket_.get_executor());
                Logger::instance().debug("Proxying: " + req->method + " " + host + ":" + upstreamPort_ + req->url);
                connectUpstream();
            }
        } else {
            // Parser returned true but no complete request yet — keep reading
            readClient();
        }
    } else {
        // feedRequest returned false — could be partial data or parse error
        // If we have accumulated data, it might be an incomplete request — keep reading
        if (clientData_.size() < 65536) {
            readClient();
        } else {
            Logger::instance().error("Request too large or unparseable — closing");
            sendErrorResponse(400, "Bad Request");
        }
    }
}

void Session::connectUpstream() {
    auto self = shared_from_this();
    Logger::instance().debug("Resolving DNS for " + upstreamHost_ + ":" + upstreamPort_);
    resolver_.async_resolve(upstreamHost_, upstreamPort_,
        [this, self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
            Logger::instance().debug("DNS resolution completed for " + upstreamHost_ + " (ec: " + std::to_string(ec.value()) + ")");
            if (ec) {
                Logger::instance().error("DNS resolution failed for " + upstreamHost_ + ": " + ec.message());
                sendErrorResponse(502, "Bad Gateway");
                return;
            }
            handleUpstreamConnect(ec, results);
        });
}

void Session::handleUpstreamConnect(boost::system::error_code /*ec*/, boost::asio::ip::tcp::resolver::results_type results) {
    auto self = shared_from_this();
    Logger::instance().debug("Connecting upstream...");
    boost::asio::async_connect(serverSocket_, results,
        [this, self](boost::system::error_code ec, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
            Logger::instance().debug("Upstream connect completed (ec: " + std::to_string(ec.value()) + ")");
            if (ec) {
                Logger::instance().error("Upstream connect failed: " + ec.message());
                if (isConnect_) {
                    sendErrorResponse(502, "Bad Gateway");
                } else {
                    sendErrorResponse(502, "Bad Gateway");
                }
                return;
            }
            if (isConnect_) {
                writeClient("HTTP/1.1 200 Connection Established\r\n\r\n");
                // Start pumping data both ways
                readClient();
                readUpstream();
            } else {
                writeUpstream();
            }
        });
}

void Session::writeUpstream() {
    auto self = shared_from_this();
    std::string serialized = currentTransaction_.request->serialize();
    // Store serialized data to keep it alive during async_write
    serverData_ = std::move(serialized);
    Logger::instance().debug("Writing upstream (" + std::to_string(serverData_.size()) + " bytes)");
    boost::asio::async_write(serverSocket_, boost::asio::buffer(serverData_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            Logger::instance().debug("Upstream write completed (ec: " + std::to_string(ec.value()) + ", bytes: " + std::to_string(bytes_transferred) + ")");
            handleUpstreamWrite(ec, bytes_transferred);
        });
}

void Session::handleUpstreamWrite(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        Logger::instance().error("Upstream write failed: " + ec.message());
        sendErrorResponse(502, "Bad Gateway");
        return;
    }
    serverData_.clear();
    readUpstream();
}

void Session::readUpstream() {
    auto self = shared_from_this();
    Logger::instance().debug("Reading upstream...");
    serverSocket_.async_read_some(boost::asio::buffer(serverBuffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            Logger::instance().debug("Upstream read completed (ec: " + std::to_string(ec.value()) + ", bytes: " + std::to_string(bytes_transferred) + ")");
            handleUpstreamRead(ec, bytes_transferred);
        });
}

void Session::handleUpstreamRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::asio::error::eof) {
            // Server closed connection — if we haven't parsed a response yet,
            // feed what we have and emit it
            if (!currentTransaction_.response && !serverData_.empty()) {
                parser_.feedResponse(serverData_);
                if (auto res = parser_.takeResponse()) {
                    currentTransaction_.response = std::make_shared<HttpResponse>(*res);
                    if (onTransaction_) {
                        onTransaction_(currentTransaction_);
                    }
                }
            }
        } else if (ec != boost::asio::error::operation_aborted) {
            Logger::instance().error("Upstream read error: " + ec.message());
        }
        close();
        return;
    }
    resetTimer();
    std::string_view data(serverBuffer_.data(), bytes_transferred);
    std::string rawChunk(data);
    writeClient(rawChunk);

    if (isConnect_) {
        // Blind tunnel — do not parse
        readUpstream();
        return;
    }
    
    if (parser_.feedResponse(data)) {
        if (auto res = parser_.takeResponse()) {
            currentTransaction_.response = std::make_shared<HttpResponse>(*res);
            if (onTransaction_) {
                onTransaction_(currentTransaction_);
            }
            
            // Check for keep-alive
            std::string connReq = currentTransaction_.request->header("Connection");
            std::string connRes = res->header("Connection");
            bool keepAlive = false;
            
            // HTTP/1.1 defaults to keep-alive unless Connection: close
            if (currentTransaction_.request->version == "HTTP/1.1") {
                keepAlive = (connReq.find("close") == std::string::npos &&
                             connRes.find("close") == std::string::npos);
            } else {
                // HTTP/1.0 defaults to close unless Connection: keep-alive
                keepAlive = (connReq.find("keep-alive") != std::string::npos ||
                             connRes.find("keep-alive") != std::string::npos);
            }
            
            if (keepAlive) {
                parser_.reset();
                clientData_.clear();
                serverData_.clear();
                currentTransaction_ = HttpTransaction{};
                isConnect_ = false;
                
                // Keep serverSocket_ open for connection reuse.
                readClient();
            } else {
                // Not keep-alive — close after all queued response data is sent
                closeAfterWrite_ = true;
                if (writeQueue_.empty()) {
                    close();
                }
            }
        } else {
            // Response not complete yet — keep reading
            readUpstream();
        }
    } else {
        // feedResponse returned false — incomplete or error
        // For streaming responses, keep reading until server closes
        readUpstream();
    }
}

void Session::writeClient(std::string data) {
    auto self = shared_from_this();
    bool idle = writeQueue_.empty();
    writeQueue_.push_back(std::move(data));
    if (idle) {
        doClientWrite();
    }
}

void Session::doClientWrite() {
    auto self = shared_from_this();
    boost::asio::async_write(clientSocket_, boost::asio::buffer(writeQueue_.front()),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleClientWrite(ec, bytes_transferred);
        });
}

void Session::handleClientWrite(boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            Logger::instance().error("Client write failed: " + ec.message());
        }
        close();
        return;
    }
    writeQueue_.pop_front();
    if (!writeQueue_.empty()) {
        doClientWrite();
        return;
    }
    if (closeAfterWrite_) {
        close();
    }
}

void Session::writeServerBlind(std::string data) {
    auto self = shared_from_this();
    Logger::instance().debug("Blind forwarding " + std::to_string(data.size()) + " bytes to server");
    bool idle = writeServerQueue_.empty();
    writeServerQueue_.push_back(std::move(data));
    if (idle) {
        doServerWrite();
    }
}

void Session::doServerWrite() {
    auto self = shared_from_this();
    boost::asio::async_write(serverSocket_, boost::asio::buffer(writeServerQueue_.front()),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            handleServerWrite(ec, bytes_transferred);
        });
}

void Session::handleServerWrite(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (!ec) {
        Logger::instance().debug("Blind server write completed (" + std::to_string(bytes_transferred) + " bytes)");
        writeServerQueue_.pop_front();
        if (!writeServerQueue_.empty()) {
            doServerWrite();
        }
    } else if (ec != boost::asio::error::operation_aborted) {
        Logger::instance().error("Server blind write error: " + ec.message());
        close();
    }
}

void Session::sendErrorResponse(int statusCode, const std::string& statusText) {
    std::string body = "<html><body><h1>" + std::to_string(statusCode) + " " + statusText + "</h1>"
                       "<p>BurpTUI Proxy Error</p></body></html>";
    std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "Connection: close\r\n"
                           "\r\n" + body;
    
    // Log the error transaction
    currentTransaction_.response = std::make_shared<HttpResponse>();
    currentTransaction_.response->version = "HTTP/1.1";
    currentTransaction_.response->statusCode = statusCode;
    currentTransaction_.response->statusText = statusText;
    currentTransaction_.response->body = body;
    if (onTransaction_) {
        onTransaction_(currentTransaction_);
    }
    
    auto self = shared_from_this();
    closeAfterWrite_ = true;
    writeClient(response);
}

void Session::serveCaCert() {
    std::string certContent;
    X509* caCert = SslInit::instance().caCert();
    if (caCert) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (bio) {
            if (PEM_write_bio_X509(bio, caCert)) {
                char* data = nullptr;
                long len = BIO_get_mem_data(bio, &data);
                if (data && len > 0) {
                    certContent.assign(data, static_cast<size_t>(len));
                }
            }
            BIO_free(bio);
        }
    }

    if (certContent.empty()) {
        std::string certPath = SslInit::instance().caCertPath();
        std::ifstream ifs(certPath, std::ios::binary);
        if (ifs) {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            certContent = oss.str();
        }
    }

    if (certContent.empty()) {
        Logger::instance().error("serveCaCert: CA certificate not available");
        sendErrorResponse(500, "CA Certificate Not Found");
        return;
    }

    std::string response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/x-x509-ca-cert\r\n"
                           "Content-Disposition: attachment; filename=\"burptui-ca.crt\"\r\n"
                           "Content-Length: " + std::to_string(certContent.size()) + "\r\n"
                           "Connection: close\r\n"
                           "\r\n" + certContent;

    currentTransaction_.response = std::make_shared<HttpResponse>();
    currentTransaction_.response->version = "HTTP/1.1";
    currentTransaction_.response->statusCode = 200;
    currentTransaction_.response->statusText = "OK";
    if (onTransaction_) {
        onTransaction_(currentTransaction_);
    }

    closeAfterWrite_ = true;
    writeClient(response);
}

void Session::serveBurpHelpPage() {
    std::string certPath = SslInit::instance().caCertPath();
    std::string html = 
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<title>BurpTUI CA Certificate</title>"
        "<style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif;background:#14141e;color:#eee;text-align:center;padding:60px 20px;}"
        ".card{max-width:540px;margin:0 auto;background:#1e1e2d;border:1px solid #333348;border-radius:12px;padding:40px 30px;box-shadow:0 8px 30px rgba(0,0,0,0.5);}"
        "h1{color:#00ff88;margin-top:0;font-size:28px;}"
        "p{line-height:1.6;color:#bbb;font-size:15px;}"
        ".btn{display:inline-block;margin-top:20px;padding:12px 28px;background:#00b0ff;color:#000;font-weight:bold;font-size:16px;text-decoration:none;border-radius:6px;}"
        ".btn:hover{background:#00ff88;}"
        "code{background:#0d0d14;padding:4px 8px;border-radius:4px;color:#00ff88;font-size:13px;word-break:break-all;}"
        "</style></head><body>"
        "<div class=\"card\">"
        "<h1>BurpTUI Proxy</h1>"
        "<p>To intercept and inspect encrypted HTTPS traffic without browser security warnings, download and trust the BurpTUI Root CA certificate.</p>"
        "<a class=\"btn\" href=\"/cert\">Download CA Certificate</a>"
        "<p style=\"margin-top:28px;font-size:13px;color:#777;\">Certificate file path on system:<br><br><code>" + certPath + "</code></p>"
        "</div></body></html>";

    std::string response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: " + std::to_string(html.size()) + "\r\n"
                           "Connection: close\r\n"
                           "\r\n" + html;

    currentTransaction_.response = std::make_shared<HttpResponse>();
    currentTransaction_.response->version = "HTTP/1.1";
    currentTransaction_.response->statusCode = 200;
    currentTransaction_.response->statusText = "OK";
    if (onTransaction_) {
        onTransaction_(currentTransaction_);
    }

    closeAfterWrite_ = true;
    writeClient(response);
}

} // namespace BurpTUI

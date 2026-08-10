#include "proxy/Session.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "util/Logger.hpp"
#include <iostream>

namespace BurpTUI {

Session::Session(boost::asio::ip::tcp::socket clientSocket,
                 TransactionCallback onTransaction,
                 std::atomic<int>& nextId)
    : clientSocket_(std::move(clientSocket)),
      serverSocket_(clientSocket_.get_executor()),
      resolver_(clientSocket_.get_executor()),
      timer_(clientSocket_.get_executor()),
      onTransaction_(std::move(onTransaction)),
      nextId_(nextId) {
}

Session::~Session() {
    close();
}

void Session::start() {
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
        Logger::instance().debug("Session timeout — closing idle connection");
        close();
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
    clientData_.append(data);
    
    if (parser_.feedRequest(data)) {
        if (auto req = parser_.takeRequest()) {
            currentTransaction_.id = nextId_++;
            currentTransaction_.request = std::make_shared<HttpRequest>(*req);
            
            // Handle CONNECT method (HTTPS tunneling — Phase 3 full MITM)
            if (req->method == "CONNECT") {
                isConnect_ = true;
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
                currentTransaction_.port = std::stoi(port);
                currentTransaction_.is_https = true;
                
                // Respond with 200 and log the CONNECT tunnel
                currentTransaction_.response = std::make_shared<HttpResponse>();
                currentTransaction_.response->version = "HTTP/1.1";
                currentTransaction_.response->statusCode = 200;
                currentTransaction_.response->statusText = "Connection Established";
                if (onTransaction_) {
                    onTransaction_(currentTransaction_);
                }

                std::string response = "HTTP/1.1 200 Connection Established\r\n\r\n";
                writeClient(response);
                Logger::instance().info("CONNECT tunnel: " + host + ":" + port);
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

            currentTransaction_.host = host;
            currentTransaction_.port = std::stoi(port.empty() ? "80" : port);
            currentTransaction_.is_https = false;
            
            Logger::instance().debug("Proxying: " + req->method + " " + host + ":" + port + req->url);
            connectUpstream(host, port);
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

void Session::connectUpstream(const std::string& host, const std::string& port) {
    auto self = shared_from_this();
    resolver_.async_resolve(host, port,
        [this, self, host](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
            if (ec) {
                Logger::instance().error("DNS resolution failed for " + host + ": " + ec.message());
                sendErrorResponse(502, "Bad Gateway");
                return;
            }
            handleUpstreamConnect(ec, results);
        });
}

void Session::handleUpstreamConnect(boost::system::error_code /*ec*/, boost::asio::ip::tcp::resolver::results_type results) {
    auto self = shared_from_this();
    boost::asio::async_connect(serverSocket_, results,
        [this, self](boost::system::error_code ec, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
            if (ec) {
                Logger::instance().error("Upstream connect failed: " + ec.message());
                sendErrorResponse(502, "Bad Gateway");
                return;
            }
            writeUpstream();
        });
}

void Session::writeUpstream() {
    auto self = shared_from_this();
    std::string serialized = currentTransaction_.request->serialize();
    // Store serialized data to keep it alive during async_write
    serverData_ = std::move(serialized);
    boost::asio::async_write(serverSocket_, boost::asio::buffer(serverData_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
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
    serverSocket_.async_read_some(boost::asio::buffer(serverBuffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
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
    
    // Accumulate raw response data for forwarding to client
    std::string rawChunk(data);
    
    // Write raw data to client immediately (streaming)
    writeClient(rawChunk);
    
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
                
                // Close old upstream socket — new request may target different host
                boost::system::error_code ec2;
                if (serverSocket_.is_open()) {
                    serverSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec2);
                    serverSocket_.close(ec2);
                }
                // Re-create server socket for next connection
                serverSocket_ = boost::asio::ip::tcp::socket(clientSocket_.get_executor());
                
                readClient();
            } else {
                close();
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

void Session::writeClient(const std::string& data) {
    auto self = shared_from_this();
    pendingClientWrite_ = data;
    boost::asio::async_write(clientSocket_, boost::asio::buffer(pendingClientWrite_),
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
    if (isConnect_) {
        close(); // Phase 2: close after CONNECT 200 response (full MITM is Phase 3)
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
    pendingClientWrite_ = response;
    boost::asio::async_write(clientSocket_, boost::asio::buffer(pendingClientWrite_),
        [this, self](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {
            close();
        });
}

} // namespace BurpTUI

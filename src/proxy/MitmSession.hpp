#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <atomic>
#include <functional>
#include <deque>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "http/HttpTransaction.hpp"
#include "http/HttpParser.hpp"
#include "proxy/CertCache.hpp"

namespace BurpTUI {

using TransactionCallback = std::function<void(HttpTransaction)>;

/// HTTPS MITM session: intercepts CONNECT, performs dual-sided TLS handshakes,
/// enforces ALPN downgrade to HTTP/1.1, and relays decrypted HTTP traffic.
class MitmSession : public std::enable_shared_from_this<MitmSession> {
public:
    MitmSession(boost::asio::ip::tcp::socket clientSocket,
                std::string targetHost,
                std::uint16_t targetPort,
                TransactionCallback onTransaction,
                std::atomic<int>& nextId,
                std::shared_ptr<CertCache> certCache);
    ~MitmSession();

    void start();
    void close();

private:
    void connectUpstream();
    void handleUpstreamConnect(boost::system::error_code ec,
                               boost::asio::ip::tcp::resolver::results_type results);
    void sendEstablishedResponse();
    void sendErrorResponse(int statusCode, const std::string& statusText);

    void startTlsHandshakes();
    void handleClientHandshake(boost::system::error_code ec);
    void handleUpstreamHandshake(boost::system::error_code ec);
    void onBothHandshakesComplete();

    void readClient();
    void handleClientRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void writeUpstream(std::string data);
    void doUpstreamWrite();
    void handleUpstreamWrite(boost::system::error_code ec, std::size_t bytes_transferred);

    void readUpstream();
    void handleUpstreamRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void writeClient(std::string data);
    void doClientWrite();
    void handleClientWrite(boost::system::error_code ec, std::size_t bytes_transferred);

    void resetTimer();
    void handleTimeout(boost::system::error_code ec);

    boost::asio::ip::tcp::socket rawClientSocket_;
    boost::asio::ip::tcp::socket rawServerSocket_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::steady_timer timer_;

    std::string targetHost_;
    std::uint16_t targetPort_;
    TransactionCallback onTransaction_;
    std::atomic<int>& nextId_;
    std::shared_ptr<CertCache> certCache_;

    std::shared_ptr<boost::asio::ssl::context> clientSslContext_;
    boost::asio::ssl::context upstreamSslContext_{boost::asio::ssl::context::tls_client};
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> clientStream_;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> upstreamStream_;

    bool clientHandshakeDone_ = false;
    bool upstreamHandshakeDone_ = false;
    bool isClosed_ = false;

    std::array<char, 8192> clientBuffer_{};
    std::array<char, 8192> upstreamBuffer_{};

    HttpParser clientParser_;
    HttpParser upstreamParser_;

    std::string clientAccumulated_;
    std::string upstreamAccumulated_;

    std::deque<std::string> clientWriteQueue_;
    std::deque<std::string> upstreamWriteQueue_;

    HttpTransaction currentTransaction_;
    bool closeAfterWrite_ = false;
};

} // namespace BurpTUI

#pragma once
#include <memory>
#include <functional>
#include <atomic>
#include <array>
#include <string>
#include <deque>
#include <boost/asio.hpp>
#include "http/HttpTransaction.hpp"
#include "http/HttpParser.hpp"

namespace BurpTUI {

class CertCache;
using TransactionCallback = std::function<void(HttpTransaction)>;

/// Handles one client connection: reads HTTP request, connects to upstream,
/// forwards traffic bidirectionally, parses and logs the transaction.
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket clientSocket,
            TransactionCallback onTransaction,
            std::atomic<int>& nextId,
            std::shared_ptr<CertCache> certCache = nullptr);
    ~Session();
    
    void start();
    void close();

private:
    void readClient();
    void handleClientRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void connectUpstream();
    void handleUpstreamConnect(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
    void writeUpstream();
    void handleUpstreamWrite(boost::system::error_code ec, std::size_t bytes_transferred);
    void readUpstream();
    void handleUpstreamRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void writeClient(std::string data);
    void doClientWrite();
    void handleClientWrite(boost::system::error_code ec, std::size_t bytes_transferred);
    void writeServerBlind(std::string data);
    void doServerWrite();
    void handleServerWrite(boost::system::error_code ec, std::size_t bytes_transferred);
    void sendErrorResponse(int statusCode, const std::string& statusText);
    void resetTimer();
    void handleTimeout(boost::system::error_code ec);

    boost::asio::ip::tcp::socket clientSocket_;
    boost::asio::ip::tcp::socket serverSocket_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::steady_timer timer_;
    TransactionCallback onTransaction_;
    std::atomic<int>& nextId_;
    std::shared_ptr<CertCache> certCache_;

    std::array<char, 8192> clientBuffer_;
    std::array<char, 8192> serverBuffer_;
    
    HttpParser parser_;

    std::string clientData_;
    std::string serverData_;
    std::deque<std::string> writeQueue_;  ///< Serialised write queue to client
    std::deque<std::string> writeServerQueue_;  ///< Serialised write queue to server (for CONNECT tunnel)
    
    std::string upstreamHost_;
    std::string upstreamPort_;
    
    HttpTransaction currentTransaction_;
    bool isConnect_ = false;
    bool closeAfterWrite_ = false;  ///< Set by sendErrorResponse to close once write queue drains
};

} // namespace BurpTUI

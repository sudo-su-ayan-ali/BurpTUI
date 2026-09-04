#include <gtest/gtest.h>
#include "proxy/ProxyServer.hpp"
#include "proxy/MitmSession.hpp"
#include "proxy/CertGenerator.hpp"
#include "proxy/CertCache.hpp"
#include "proxy/SslInit.hpp"
#include "http/HttpTransaction.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <filesystem>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace fs = std::filesystem;
using boost::asio::ip::tcp;

class MitmSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        testCaDir_ = (fs::temp_directory_path() / "burptui_mitm_test").string();
        fs::remove_all(testCaDir_);
        ASSERT_TRUE(BurpTUI::SslInit::instance().initialize(testCaDir_));
    }

    void TearDown() override {
        fs::remove_all(testCaDir_);
    }

    std::string testCaDir_;
};

/// Simple Mock HTTPS Server that returns a fixed response and shuts down.
class MockHttpsServer {
public:
    explicit MockHttpsServer(std::uint16_t port = 0)
        : acceptor_(ioc_, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port)),
          sslCtx_(boost::asio::ssl::context::tls_server)
    {
        // Mint a server certificate for 127.0.0.1
        BurpTUI::CertGenerator gen;
        auto keyPair = gen.generate("127.0.0.1");

        sslCtx_.use_certificate_chain(boost::asio::buffer(keyPair.certPem));
        sslCtx_.use_private_key(boost::asio::buffer(keyPair.keyPem), boost::asio::ssl::context::file_format::pem);

        port_ = acceptor_.local_endpoint().port();
    }

    std::uint16_t port() const { return port_; }

    void start() {
        thread_ = std::thread([this]() {
            try {
                acceptor_.async_accept(
                    [this](boost::system::error_code ec, tcp::socket socket) {
                        if (ec) return;
                        auto sslStream = std::make_shared<boost::asio::ssl::stream<tcp::socket>>(
                            std::move(socket), sslCtx_);
                        sslStream->async_handshake(
                            boost::asio::ssl::stream_base::server,
                            [this, sslStream](boost::system::error_code ec) {
                                if (ec) return;
                                auto buf = std::make_shared<std::array<char, 1024>>();
                                sslStream->async_read_some(
                                    boost::asio::buffer(*buf),
                                    [this, sslStream, buf](boost::system::error_code ec, std::size_t) {
                                        if (ec) return;
                                        static const std::string res =
                                            "HTTP/1.1 200 OK\r\n"
                                            "Content-Type: text/plain\r\n"
                                            "Content-Length: 19\r\n"
                                            "Connection: close\r\n\r\n"
                                            "Hello from Upstream";
                                        boost::asio::async_write(
                                            *sslStream, boost::asio::buffer(res),
                                            [sslStream](boost::system::error_code, std::size_t) {
                                                boost::system::error_code ignored;
                                                sslStream->lowest_layer().close(ignored);
                                            });
                                    });
                            });
                    });
                ioc_.run();
            } catch (...) {}
        });
    }

    void stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        ioc_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    ~MockHttpsServer() {
        stop();
    }

private:
    boost::asio::io_context ioc_;
    tcp::acceptor acceptor_;
    boost::asio::ssl::context sslCtx_;
    std::uint16_t port_ = 0;
    std::thread thread_;
};

TEST_F(MitmSessionTest, InterceptsAndDecryptsHttpsTrafficEndToEnd) {
    // 1. Start Mock HTTPS upstream server
    MockHttpsServer upstream(0);
    upstream.start();
    std::uint16_t upstreamPort = upstream.port();
    ASSERT_GT(upstreamPort, 0);

    // 2. Start ProxyServer with transaction capture
    std::mutex txMutex;
    std::condition_variable txCv;
    std::vector<BurpTUI::HttpTransaction> capturedTxs;

    BurpTUI::CertGenerator certGen;
    auto certCache = std::make_shared<BurpTUI::CertCache>(certGen);

    // Find a free port for ProxyServer
    boost::asio::io_context probeIoc;
    tcp::acceptor probeAcceptor(probeIoc, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    std::uint16_t proxyPort = probeAcceptor.local_endpoint().port();
    probeAcceptor.close();

    BurpTUI::ProxyServer proxy(
        "127.0.0.1", proxyPort,
        [&](BurpTUI::HttpTransaction tx) {
            std::lock_guard<std::mutex> lock(txMutex);
            capturedTxs.push_back(std::move(tx));
            txCv.notify_all();
        },
        certCache
    );
    proxy.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 3. Client connects to ProxyServer
    boost::asio::io_context clientIoc;
    tcp::socket clientSocket(clientIoc);
    clientSocket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), proxyPort));

    // Send CONNECT request
    std::string connectReq = "CONNECT 127.0.0.1:" + std::to_string(upstreamPort) + " HTTP/1.1\r\n"
                             "Host: 127.0.0.1:" + std::to_string(upstreamPort) + "\r\n\r\n";
    boost::asio::write(clientSocket, boost::asio::buffer(connectReq));

    // Read 200 Connection Established
    std::array<char, 256> respBuf{};
    boost::system::error_code ec;
    size_t n = clientSocket.read_some(boost::asio::buffer(respBuf), ec);
    ASSERT_FALSE(ec);
    std::string establishedResp(respBuf.data(), n);
    EXPECT_NE(establishedResp.find("200 Connection Established"), std::string::npos);

    // 4. Upgrade client socket to TLS client stream
    boost::asio::ssl::context clientCtx(boost::asio::ssl::context::tls_client);
    clientCtx.set_verify_mode(boost::asio::ssl::verify_none);

    boost::asio::ssl::stream<tcp::socket> clientSslStream(std::move(clientSocket), clientCtx);
    clientSslStream.handshake(boost::asio::ssl::stream_base::client);

    // 5. Send decrypted HTTP GET request through the TLS tunnel
    std::string httpsReq = "GET /mitm-test HTTP/1.1\r\n"
                           "Host: 127.0.0.1:" + std::to_string(upstreamPort) + "\r\n"
                           "User-Agent: BurpTUI-Test/1.0\r\n"
                           "Connection: close\r\n\r\n";
    boost::asio::write(clientSslStream, boost::asio::buffer(httpsReq));

    // 6. Read decrypted HTTP response through the TLS tunnel
    std::array<char, 1024> decryptedBuf{};
    size_t bytesRead = clientSslStream.read_some(boost::asio::buffer(decryptedBuf), ec);
    std::string decryptedResponse(decryptedBuf.data(), bytesRead);
    EXPECT_NE(decryptedResponse.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(decryptedResponse.find("Hello from Upstream"), std::string::npos);

    // 7. Verify transaction recorded in proxy
    {
        std::unique_lock<std::mutex> lock(txMutex);
        txCv.wait_for(lock, std::chrono::seconds(3), [&]() {
            // We expect at least the decrypted transaction (or connect + decrypted)
            for (const auto& tx : capturedTxs) {
                if (tx.is_https && tx.request && tx.request->url == "/mitm-test") {
                    return true;
                }
            }
            return false;
        });
    }

    std::lock_guard<std::mutex> lock(txMutex);
    bool foundMitmTx = false;
    for (const auto& tx : capturedTxs) {
        if (tx.is_https && tx.request && tx.request->url == "/mitm-test") {
            foundMitmTx = true;
            EXPECT_EQ(tx.host, "127.0.0.1");
            EXPECT_EQ(tx.port, upstreamPort);
            EXPECT_EQ(tx.request->method, "GET");
            EXPECT_EQ(tx.request->header("User-Agent"), "BurpTUI-Test/1.0");
            ASSERT_NE(tx.response, nullptr);
            EXPECT_EQ(tx.response->statusCode, 200);
            EXPECT_EQ(tx.response->body, "Hello from Upstream");
            break;
        }
    }
    EXPECT_TRUE(foundMitmTx);

    proxy.stop();
    upstream.stop();
}

TEST_F(MitmSessionTest, HandlesTlsHandshakeFailureGracefully) {
    // Test that client abruptly closing or sending garbage doesn't crash proxy
    MockHttpsServer upstream(0);
    upstream.start();

    boost::asio::io_context probeIoc;
    tcp::acceptor probeAcceptor(probeIoc, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    std::uint16_t proxyPort = probeAcceptor.local_endpoint().port();
    probeAcceptor.close();

    BurpTUI::ProxyServer proxy("127.0.0.1", proxyPort);
    proxy.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    boost::asio::io_context clientIoc;
    tcp::socket clientSocket(clientIoc);
    clientSocket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), proxyPort));

    std::string connectReq = "CONNECT 127.0.0.1:" + std::to_string(upstream.port()) + " HTTP/1.1\r\n\r\n";
    boost::asio::write(clientSocket, boost::asio::buffer(connectReq));

    std::array<char, 256> respBuf{};
    boost::system::error_code ec;
    clientSocket.read_some(boost::asio::buffer(respBuf), ec);

    // Send junk data instead of TLS ClientHello
    std::string junk = "NOT_A_VALID_TLS_CLIENT_HELLO_PACKET_JUST_GARBAGE";
    boost::asio::write(clientSocket, boost::asio::buffer(junk), ec);

    // Wait a brief moment — proxy should reject handshake and close without crashing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(proxy.isRunning());
    proxy.stop();
    upstream.stop();
}

TEST_F(MitmSessionTest, HandlesUpstreamConnectionFailure) {
    // Test CONNECT to non-existent upstream port
    boost::asio::io_context probeIoc;
    tcp::acceptor probeAcceptor(probeIoc, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    std::uint16_t proxyPort = probeAcceptor.local_endpoint().port();
    probeAcceptor.close();

    BurpTUI::ProxyServer proxy("127.0.0.1", proxyPort);
    proxy.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    boost::asio::io_context clientIoc;
    tcp::socket clientSocket(clientIoc);
    clientSocket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), proxyPort));

    // Connect to port 1 (which will be refused)
    std::string connectReq = "CONNECT 127.0.0.1:1 HTTP/1.1\r\nHost: 127.0.0.1:1\r\n\r\n";
    boost::asio::write(clientSocket, boost::asio::buffer(connectReq));

    std::array<char, 256> respBuf{};
    boost::system::error_code ec;
    size_t n = clientSocket.read_some(boost::asio::buffer(respBuf), ec);
    std::string errResp(respBuf.data(), n);
    EXPECT_NE(errResp.find("502 Bad Gateway"), std::string::npos);

    proxy.stop();
}

TEST_F(MitmSessionTest, ServesCaCertAtBurpCertEndpoint) {
    boost::asio::io_context probeIoc;
    tcp::acceptor probeAcceptor(probeIoc, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    std::uint16_t proxyPort = probeAcceptor.local_endpoint().port();
    probeAcceptor.close();

    BurpTUI::ProxyServer proxy("127.0.0.1", proxyPort);
    proxy.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Request http://burp/cert
    boost::asio::io_context clientIoc;
    tcp::socket clientSocket(clientIoc);
    clientSocket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), proxyPort));

    std::string req = "GET http://burp/cert HTTP/1.1\r\nHost: burp\r\nConnection: close\r\n\r\n";
    boost::asio::write(clientSocket, boost::asio::buffer(req));

    std::array<char, 4096> respBuf{};
    boost::system::error_code ec;
    size_t n = clientSocket.read_some(boost::asio::buffer(respBuf), ec);
    std::string resp(respBuf.data(), n);

    EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("application/x-x509-ca-cert"), std::string::npos);
    EXPECT_NE(resp.find("BEGIN CERTIFICATE"), std::string::npos);

    proxy.stop();
}

#pragma once
#include <string>
#include <openssl/x509.h>
#include <openssl/evp.h>

namespace BurpTUI {

/// Application-wide OpenSSL initialization and Root CA management.
/// Call SslInit::instance().initialize() once at startup.
class SslInit {
public:
    static SslInit& instance();

    /// Initialize OpenSSL, create CA dir, generate or load Root CA.
    /// @param caDir Directory to store CA cert+key (default: "./ca")
    /// @return true on success
    bool initialize(const std::string& caDir = "./ca");

    /// Get the loaded Root CA certificate (owned by SslInit, do NOT free)
    X509*    caCert() const;
    /// Get the loaded Root CA private key (owned by SslInit, do NOT free)
    EVP_PKEY* caKey() const;

    /// Get the filesystem paths
    const std::string& caCertPath() const;
    const std::string& caKeyPath() const;
    const std::string& caDir() const;

    /// Check if initialization succeeded
    bool isInitialized() const;

    ~SslInit();
    SslInit(const SslInit&) = delete;
    SslInit& operator=(const SslInit&) = delete;

private:
    SslInit() = default;

    bool initOpenSSL();
    bool ensureCaDirectory(const std::string& dir);
    bool generateRootCA(const std::string& certPath, const std::string& keyPath);
    bool loadRootCA(const std::string& certPath, const std::string& keyPath);

    X509*     caCert_  = nullptr;
    EVP_PKEY* caKey_   = nullptr;
    std::string caDir_;
    std::string caCertPath_;
    std::string caKeyPath_;
    bool initialized_ = false;
};

} // namespace BurpTUI

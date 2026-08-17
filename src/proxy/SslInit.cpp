#include "proxy/SslInit.hpp"
#include "util/Logger.hpp"

#include <filesystem>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

namespace fs = std::filesystem;

namespace BurpTUI {

SslInit& SslInit::instance() {
    static SslInit instance;
    return instance;
}

SslInit::~SslInit() {
    if (caCert_) {
        X509_free(caCert_);
        caCert_ = nullptr;
    }
    if (caKey_) {
        EVP_PKEY_free(caKey_);
        caKey_ = nullptr;
    }
}

bool SslInit::initialize(const std::string& caDir) {
    if (initialized_) {
        Logger::instance().warn("SslInit is already initialized.");
        return true;
    }

    caDir_ = caDir;
    caCertPath_ = (fs::path(caDir) / "ca.crt").string();
    caKeyPath_  = (fs::path(caDir) / "ca.key").string();

    if (!initOpenSSL()) {
        return false;
    }

    if (!ensureCaDirectory(caDir_)) {
        return false;
    }

    if (fs::exists(caCertPath_) && fs::exists(caKeyPath_)) {
        Logger::instance().info("Existing CA files found, loading...");
        if (!loadRootCA(caCertPath_, caKeyPath_)) {
            Logger::instance().error("Failed to load existing CA files.");
            return false;
        }
    } else {
        Logger::instance().info("CA files not found, generating new Root CA...");
        if (!generateRootCA(caCertPath_, caKeyPath_)) {
            Logger::instance().error("Failed to generate Root CA.");
            return false;
        }
    }

    initialized_ = true;
    return true;
}

bool SslInit::initOpenSSL() {
    uint64_t opts = OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS;
    if (OPENSSL_init_ssl(opts, nullptr) != 1) {
        char errBuf[256];
        ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
        Logger::instance().error("Failed to initialize OpenSSL: " + std::string(errBuf));
        return false;
    }
    Logger::instance().info("OpenSSL initialized successfully (OpenSSL 3.x API).");
    return true;
}

bool SslInit::ensureCaDirectory(const std::string& dir) {
    try {
        if (!fs::exists(dir)) {
            if (fs::create_directories(dir)) {
                Logger::instance().info("Created CA directory: " + dir);
            } else {
                Logger::instance().error("Failed to create CA directory: " + dir);
                return false;
            }
        } else {
            Logger::instance().debug("CA directory exists: " + dir);
        }
    } catch (const fs::filesystem_error& e) {
        Logger::instance().error("Filesystem error while ensuring CA directory: " + std::string(e.what()));
        return false;
    }
    return true;
}

bool SslInit::generateRootCA(const std::string& certPath, const std::string& keyPath) {
    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    if (!pkey) {
        Logger::instance().error("Failed to generate RSA 2048 keypair.");
        return false;
    }

    X509* cert = X509_new();
    if (!cert) {
        Logger::instance().error("Failed to create X509 object.");
        EVP_PKEY_free(pkey);
        return false;
    }

    X509_set_version(cert, 2); // X509 v3
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 10L * 365 * 24 * 60 * 60);
    X509_set_pubkey(cert, pkey);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (const unsigned char*)"XX", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (const unsigned char*)"BurpTUI", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L",  MBSTRING_ASC, (const unsigned char*)"Proxy", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (const unsigned char*)"BurpTUI", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"BurpTUI Root CA", -1, -1, 0);

    X509_set_issuer_name(cert, name);

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

    auto add_ext = [&](int nid, const char* value) {
        X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
        if (ext) {
            X509_add_ext(cert, ext, -1);
            X509_EXTENSION_free(ext);
        } else {
            Logger::instance().warn("Failed to create extension NID " + std::to_string(nid));
        }
    };

    add_ext(NID_basic_constraints, "critical,CA:TRUE");
    add_ext(NID_key_usage, "critical,keyCertSign,cRLSign");
    add_ext(NID_subject_key_identifier, "hash");

    if (X509_sign(cert, pkey, EVP_sha256()) == 0) {
        Logger::instance().error("Failed to self-sign the Root CA certificate.");
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return false;
    }

    BIO* pkeyBio = BIO_new_file(keyPath.c_str(), "w");
    if (!pkeyBio) {
        Logger::instance().error("Failed to open key file for writing: " + keyPath);
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return false;
    }
    PEM_write_bio_PrivateKey(pkeyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    BIO_free(pkeyBio);

    BIO* certBio = BIO_new_file(certPath.c_str(), "w");
    if (!certBio) {
        Logger::instance().error("Failed to open cert file for writing: " + certPath);
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return false;
    }
    PEM_write_bio_X509(certBio, cert);
    BIO_free(certBio);

    caCert_ = cert;
    caKey_ = pkey;

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen;
    if (X509_digest(cert, EVP_sha256(), md, &mdLen)) {
        std::string fingerprint = "SHA-256 Fingerprint: ";
        char buf[4];
        for (unsigned int i = 0; i < mdLen; ++i) {
            snprintf(buf, sizeof(buf), "%02X%s", md[i], (i == mdLen - 1) ? "" : ":");
            fingerprint += buf;
        }
        Logger::instance().info(fingerprint);
    }

    Logger::instance().info("Root CA generated and saved to " + caDir_);
    return true;
}

bool SslInit::loadRootCA(const std::string& certPath, const std::string& keyPath) {
    BIO* certBio = BIO_new_file(certPath.c_str(), "r");
    if (!certBio) {
        Logger::instance().error("Failed to open cert file for reading: " + certPath);
        return false;
    }
    caCert_ = PEM_read_bio_X509(certBio, nullptr, nullptr, nullptr);
    BIO_free(certBio);

    if (!caCert_) {
        Logger::instance().error("Failed to parse Root CA certificate from " + certPath);
        return false;
    }

    BIO* keyBio = BIO_new_file(keyPath.c_str(), "r");
    if (!keyBio) {
        Logger::instance().error("Failed to open key file for reading: " + keyPath);
        X509_free(caCert_);
        caCert_ = nullptr;
        return false;
    }
    caKey_ = PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr);
    BIO_free(keyBio);

    if (!caKey_) {
        Logger::instance().error("Failed to parse Root CA private key from " + keyPath);
        X509_free(caCert_);
        caCert_ = nullptr;
        return false;
    }

    if (X509_check_private_key(caCert_, caKey_) != 1) {
        Logger::instance().error("Root CA certificate and private key do not match!");
        X509_free(caCert_);
        EVP_PKEY_free(caKey_);
        caCert_ = nullptr;
        caKey_ = nullptr;
        return false;
    }

    Logger::instance().info("Successfully loaded existing Root CA from " + caDir_);
    return true;
}

X509* SslInit::caCert() const { return caCert_; }
EVP_PKEY* SslInit::caKey() const { return caKey_; }
const std::string& SslInit::caCertPath() const { return caCertPath_; }
const std::string& SslInit::caKeyPath() const { return caKeyPath_; }
const std::string& SslInit::caDir() const { return caDir_; }
bool SslInit::isInitialized() const { return initialized_; }

} // namespace BurpTUI

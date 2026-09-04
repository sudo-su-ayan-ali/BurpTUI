#include "proxy/CertGenerator.hpp"
#include "proxy/SslInit.hpp"
#include "util/Logger.hpp"

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>

namespace BurpTUI {

struct CertGenerator::Impl {
    std::string caKeyPath;
    std::string caCertPath;
    X509* localCaCert = nullptr;
    EVP_PKEY* localCaKey = nullptr;

    ~Impl() {
        if (localCaCert) X509_free(localCaCert);
        if (localCaKey) EVP_PKEY_free(localCaKey);
    }
};

CertGenerator::CertGenerator()
    : impl_(std::make_unique<Impl>())
{}

CertGenerator::CertGenerator(const std::string& caKeyPath,
                             const std::string& caCertPath)
    : impl_(std::make_unique<Impl>())
{
    impl_->caKeyPath = caKeyPath;
    impl_->caCertPath = caCertPath;
}

CertGenerator::~CertGenerator() = default;

CertGenerator::CertKeyPair CertGenerator::generate(const std::string& hostname) {
    if (hostname.empty()) {
        Logger::instance().error("CertGenerator: empty hostname provided");
        return {};
    }

    X509* caCert = nullptr;
    EVP_PKEY* caKey = nullptr;

    // 1. Try to get Root CA from SslInit singleton
    if (SslInit::instance().isInitialized()) {
        caCert = SslInit::instance().caCert();
        caKey = SslInit::instance().caKey();
    }

    // 2. Fallback: If SslInit is not initialized but custom paths were provided, load them locally
    if ((!caCert || !caKey) && impl_ && !impl_->caCertPath.empty() && !impl_->caKeyPath.empty()) {
        if (!impl_->localCaCert) {
            BIO* certBio = BIO_new_file(impl_->caCertPath.c_str(), "r");
            if (certBio) {
                impl_->localCaCert = PEM_read_bio_X509(certBio, nullptr, nullptr, nullptr);
                BIO_free(certBio);
            }
        }
        if (!impl_->localCaKey) {
            BIO* keyBio = BIO_new_file(impl_->caKeyPath.c_str(), "r");
            if (keyBio) {
                impl_->localCaKey = PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr);
                BIO_free(keyBio);
            }
        }
        caCert = impl_->localCaCert;
        caKey = impl_->localCaKey;
    }

    if (!caCert || !caKey) {
        Logger::instance().error("CertGenerator: Root CA certificate or key unavailable.");
        return {};
    }

    // 3. Generate 2048-bit RSA key for leaf certificate
    EVP_PKEY* leafKey = EVP_RSA_gen(2048);
    if (!leafKey) {
        Logger::instance().error("CertGenerator: Failed to generate RSA key for host: " + hostname);
        return {};
    }

    // 4. Create X.509 certificate structure
    X509* cert = X509_new();
    if (!cert) {
        Logger::instance().error("CertGenerator: Failed to allocate X509 structure");
        EVP_PKEY_free(leafKey);
        return {};
    }

    X509_set_version(cert, 2); // X509 v3

    // Set serial number from high-entropy random bytes
    uint64_t serialNum = 0;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&serialNum), sizeof(serialNum)) != 1) {
        serialNum = 1000 + (std::hash<std::string>{}(hostname) % 1000000);
    }
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert), serialNum);

    // Set validity: 1 day before today to 1 year from now
    X509_gmtime_adj(X509_get_notBefore(cert), -86400L);
    X509_gmtime_adj(X509_get_notAfter(cert), 365L * 86400L);

    // Assign leaf public key
    X509_set_pubkey(cert, leafKey);

    // Set Subject Name
    X509_NAME* subjectName = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(subjectName, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>(hostname.c_str()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(subjectName, "O", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("BurpTUI Dynamic CA"), -1, -1, 0);

    // Set Issuer Name from Root CA certificate
    X509_set_issuer_name(cert, X509_get_subject_name(caCert));

    // 5. Add X.509 v3 Extensions
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, caCert, cert, nullptr, nullptr, 0);

    auto addExt = [&](int nid, const char* value) {
        X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
        if (ext) {
            X509_add_ext(cert, ext, -1);
            X509_EXTENSION_free(ext);
        } else {
            Logger::instance().warn("CertGenerator: Failed to add extension NID " + std::to_string(nid));
        }
    };

    addExt(NID_basic_constraints, "critical,CA:FALSE");
    addExt(NID_key_usage, "digitalSignature,keyEncipherment");
    addExt(NID_ext_key_usage, "serverAuth,clientAuth");

    // Add Subject Alternative Name (SAN) extension
    std::string sanStr = "DNS:" + hostname;
    addExt(NID_subject_alt_name, sanStr.c_str());

    addExt(NID_subject_key_identifier, "hash");
    addExt(NID_authority_key_identifier, "keyid:always,issuer");

    // 6. Sign leaf certificate with Root CA private key
    if (X509_sign(cert, caKey, EVP_sha256()) == 0) {
        Logger::instance().error("CertGenerator: Failed to sign certificate for host: " + hostname);
        X509_free(cert);
        EVP_PKEY_free(leafKey);
        return {};
    }

    // 7. Serialize Cert and Private Key to PEM strings
    CertKeyPair result;

    BIO* certBio = BIO_new(BIO_s_mem());
    if (certBio && PEM_write_bio_X509(certBio, cert)) {
        BUF_MEM* mem = nullptr;
        BIO_get_mem_ptr(certBio, &mem);
        if (mem && mem->data && mem->length > 0) {
            result.certPem.assign(mem->data, mem->length);
        }
    }
    if (certBio) BIO_free(certBio);

    BIO* keyBio = BIO_new(BIO_s_mem());
    if (keyBio && PEM_write_bio_PrivateKey(keyBio, leafKey, nullptr, nullptr, 0, nullptr, nullptr)) {
        BUF_MEM* mem = nullptr;
        BIO_get_mem_ptr(keyBio, &mem);
        if (mem && mem->data && mem->length > 0) {
            result.keyPem.assign(mem->data, mem->length);
        }
    }
    if (keyBio) BIO_free(keyBio);

    X509_free(cert);
    EVP_PKEY_free(leafKey);

    if (result.certPem.empty() || result.keyPem.empty()) {
        Logger::instance().error("CertGenerator: Failed to serialize PEM for host: " + hostname);
        return {};
    }

    Logger::instance().debug("CertGenerator: Successfully minted certificate for " + hostname);
    return result;
}

} // namespace BurpTUI

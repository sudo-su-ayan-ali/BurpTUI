#include "proxy/CertCache.hpp"
#include "util/Logger.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace BurpTUI {

CertCache::CertCache(CertGenerator& generator, std::size_t maxEntries)
    : gen_(generator), maxEntries_(maxEntries)
{}

CertCache::~CertCache() = default;

std::shared_ptr<boost::asio::ssl::context> CertCache::createSslContext(const CertGenerator::CertKeyPair& pair) {
    if (pair.certPem.empty() || pair.keyPem.empty()) {
        Logger::instance().error("CertCache: Cannot create SSL context from empty certificate/key");
        return nullptr;
    }

    auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);

    ctx->set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::single_dh_use
    );

    boost::system::error_code ec;
    ctx->use_certificate_chain(boost::asio::const_buffer(pair.certPem.data(), pair.certPem.size()), ec);
    if (ec) {
        Logger::instance().error("CertCache: Failed to load certificate chain: " + ec.message());
        return nullptr;
    }

    ctx->use_private_key(boost::asio::const_buffer(pair.keyPem.data(), pair.keyPem.size()),
                         boost::asio::ssl::context::file_format::pem, ec);
    if (ec) {
        Logger::instance().error("CertCache: Failed to load private key: " + ec.message());
        return nullptr;
    }

    // Configure ALPN on server SSL context: force http/1.1 downgrade
    static const unsigned char alpnProtos[] = "\x08http/1.1";
    SSL_CTX_set_alpn_protos(ctx->native_handle(), alpnProtos, sizeof(alpnProtos) - 1);

    SSL_CTX_set_alpn_select_cb(ctx->native_handle(), [](SSL* /*ssl*/,
                                                         const unsigned char** out,
                                                         unsigned char* outlen,
                                                         const unsigned char* in,
                                                         unsigned int inlen,
                                                         void* /*arg*/) -> int {
        static const unsigned char serverProtos[] = "\x08http/1.1";
        int status = SSL_select_next_proto((unsigned char**)out, outlen,
                                           serverProtos, sizeof(serverProtos) - 1,
                                           in, inlen);
        if (status == OPENSSL_NPN_NEGOTIATED) {
            return SSL_TLSEXT_ERR_OK;
        }
        return SSL_TLSEXT_ERR_NOACK;
    }, nullptr);

    return ctx;
}

CertCache::Entry CertCache::getEntry(const std::string& hostname) {
    if (hostname.empty()) {
        Logger::instance().error("CertCache: empty hostname passed to getEntry()");
        return {};
    }

    // 1. Fast path: concurrent shared lock on cache hit
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        auto it = cache_.find(hostname);
        if (it != cache_.end()) {
            return { it->second.keyPair, it->second.context };
        }
    }

    // 2. Slow path: exclusive write lock on cache miss
    std::unique_lock<std::shared_mutex> writeLock(mutex_);

    // Double-check if another thread minted the certificate while waiting for the lock
    auto it = cache_.find(hostname);
    if (it != cache_.end()) {
        lruList_.erase(it->second.lruIt);
        lruList_.push_front(hostname);
        it->second.lruIt = lruList_.begin();
        return { it->second.keyPair, it->second.context };
    }

    // Mint new certificate using CertGenerator
    auto keyPair = gen_.generate(hostname);
    if (keyPair.certPem.empty() || keyPair.keyPem.empty()) {
        Logger::instance().error("CertCache: Failed to generate certificate for: " + hostname);
        return { keyPair, nullptr };
    }

    auto sslCtx = createSslContext(keyPair);

    // Evict oldest entry if capacity is reached
    if (maxEntries_ > 0 && cache_.size() >= maxEntries_ && !lruList_.empty()) {
        std::string oldest = lruList_.back();
        lruList_.pop_back();
        cache_.erase(oldest);
        Logger::instance().debug("CertCache: Evicted oldest cert for: " + oldest);
    }

    if (maxEntries_ > 0) {
        lruList_.push_front(hostname);
        CacheItem item{
            keyPair,
            sslCtx,
            lruList_.begin()
        };
        cache_.emplace(hostname, std::move(item));
    }

    Logger::instance().debug("CertCache: Cached SSL context for " + hostname);
    return { keyPair, sslCtx };
}

std::shared_ptr<boost::asio::ssl::context> CertCache::getContext(const std::string& hostname) {
    return getEntry(hostname).sslContext;
}

CertGenerator::CertKeyPair CertCache::get(const std::string& hostname) {
    return getEntry(hostname).certKeyPair;
}

bool CertCache::contains(const std::string& hostname) const {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    return cache_.find(hostname) != cache_.end();
}

std::size_t CertCache::size() const {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    return cache_.size();
}

std::size_t CertCache::maxEntries() const {
    return maxEntries_;
}

void CertCache::clear() {
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    cache_.clear();
    lruList_.clear();
}

void CertCache::touch(const std::string& hostname) {
    std::unique_lock<std::shared_mutex> writeLock(mutex_);
    auto it = cache_.find(hostname);
    if (it != cache_.end()) {
        lruList_.erase(it->second.lruIt);
        lruList_.push_front(hostname);
        it->second.lruIt = lruList_.begin();
    }
}

} // namespace BurpTUI

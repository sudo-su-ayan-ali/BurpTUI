#pragma once
#include "proxy/CertGenerator.hpp"
#include <boost/asio/ssl.hpp>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <list>

namespace BurpTUI {

/// Thread-safe LRU cache for generated leaf certificates and SSL contexts.
class CertCache {
public:
    struct Entry {
        CertGenerator::CertKeyPair certKeyPair;
        std::shared_ptr<boost::asio::ssl::context> sslContext;
    };

    explicit CertCache(CertGenerator& generator, std::size_t maxEntries = 256);
    ~CertCache();

    /// Retrieve the SSL context for a hostname, generating and caching if needed.
    std::shared_ptr<boost::asio::ssl::context> getContext(const std::string& hostname);

    /// Retrieve the CertKeyPair for a hostname, generating and caching if needed.
    CertGenerator::CertKeyPair get(const std::string& hostname);

    /// Retrieve the full Entry (keypair + ssl context), generating and caching if needed.
    Entry getEntry(const std::string& hostname);

    /// Check if a hostname is already cached (read-only, no generation).
    bool contains(const std::string& hostname) const;

    /// Current number of cached items.
    std::size_t size() const;

    /// Clear all cached certificates.
    void clear();

    /// Maximum capacity.
    std::size_t maxEntries() const;

    /// Explicitly touch/promote a hostname to the front of the LRU queue.
    void touch(const std::string& hostname);

private:
    std::shared_ptr<boost::asio::ssl::context> createSslContext(const CertGenerator::CertKeyPair& pair);

    CertGenerator&                                           gen_;
    std::size_t                                              maxEntries_;
    mutable std::shared_mutex                                mutex_;

    struct CacheItem {
        CertGenerator::CertKeyPair keyPair;
        std::shared_ptr<boost::asio::ssl::context> context;
        std::list<std::string>::iterator lruIt;
    };

    std::list<std::string>                                   lruList_;
    std::unordered_map<std::string, CacheItem>               cache_;
};

} // namespace BurpTUI

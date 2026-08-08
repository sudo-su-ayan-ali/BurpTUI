#pragma once
#include "CertGenerator.hpp"
#include <mutex>
#include <string>
#include <unordered_map>

namespace BurpTUI {

/// Thread-safe LRU cache for generated leaf certificates.
class CertCache {
public:
    explicit CertCache(CertGenerator& generator, std::size_t maxEntries = 256);

    CertGenerator::CertKeyPair get(const std::string& hostname);

private:
    CertGenerator&                                           gen_;
    std::size_t                                              maxEntries_;
    std::mutex                                               mtx_;
    std::unordered_map<std::string, CertGenerator::CertKeyPair> cache_;
};

} // namespace BurpTUI

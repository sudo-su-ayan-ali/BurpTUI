#pragma once
#include <memory>
#include <string>
#include <cstdint>

namespace BurpTUI {

class CertGenerator {
public:
    struct CertKeyPair {
        std::string certPem;
        std::string keyPem;
    };

    explicit CertGenerator(const std::string& caKeyPath,
                           const std::string& caCertPath);
    ~CertGenerator();

    /// Generate a leaf cert signed by the CA for the given hostname.
    CertKeyPair generate(const std::string& hostname);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace BurpTUI

#include "proxy/CertGenerator.hpp"

namespace BurpTUI {

struct CertGenerator::Impl {
    std::string caKeyPath;
    std::string caCertPath;
};

CertGenerator::CertGenerator(const std::string& caKeyPath,
                              const std::string& caCertPath)
    : impl_(std::make_unique<Impl>(caKeyPath, caCertPath))
{}

CertGenerator::~CertGenerator() { }

CertGenerator::CertKeyPair CertGenerator::generate(const std::string& /*hostname*/) {
    // TODO: OpenSSL X.509 signing — Phase 2
    return {};
}

} // namespace BurpTUI

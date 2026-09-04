#include <gtest/gtest.h>
#include "proxy/CertGenerator.hpp"
#include "proxy/SslInit.hpp"
#include <filesystem>

namespace fs = std::filesystem;

class CertGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        testCaDir_ = (fs::temp_directory_path() / "burptui_cert_gen_test").string();
        fs::remove_all(testCaDir_);
    }

    void TearDown() override {
        fs::remove_all(testCaDir_);
    }

    std::string testCaDir_;
};

TEST_F(CertGeneratorTest, UninitializedCaReturnsEmpty) {
    BurpTUI::CertGenerator gen("nonexistent.key", "nonexistent.crt");
    auto pair = gen.generate("example.com");
    EXPECT_TRUE(pair.certPem.empty());
    EXPECT_TRUE(pair.keyPem.empty());
}

TEST_F(CertGeneratorTest, GeneratesValidHostCertWithSslInit) {
    ASSERT_TRUE(BurpTUI::SslInit::instance().initialize(testCaDir_));

    BurpTUI::CertGenerator gen;
    auto pair = gen.generate("example.com");

    EXPECT_FALSE(pair.certPem.empty());
    EXPECT_FALSE(pair.keyPem.empty());

    EXPECT_NE(pair.certPem.find("-----BEGIN CERTIFICATE-----"), std::string::npos);
    EXPECT_NE(pair.certPem.find("-----END CERTIFICATE-----"), std::string::npos);

    EXPECT_NE(pair.keyPem.find("-----BEGIN PRIVATE KEY-----"), std::string::npos);
    EXPECT_NE(pair.keyPem.find("-----END PRIVATE KEY-----"), std::string::npos);
}

TEST_F(CertGeneratorTest, GeneratesCertForSubdomains) {
    ASSERT_TRUE(BurpTUI::SslInit::instance().initialize(testCaDir_));

    BurpTUI::CertGenerator gen;
    auto pair1 = gen.generate("api.target.com");
    auto pair2 = gen.generate("auth.target.com");

    EXPECT_FALSE(pair1.certPem.empty());
    EXPECT_FALSE(pair2.certPem.empty());

    // Certificates for different domains should have different content
    EXPECT_NE(pair1.certPem, pair2.certPem);
}

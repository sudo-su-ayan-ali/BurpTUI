#include <gtest/gtest.h>
#include "proxy/CertGenerator.hpp"

TEST(CertGenerator, StubReturnsEmpty) {
    // CA files don't exist yet — just verify no crash
    BurpTUI::CertGenerator gen("ca.key", "ca.crt");
    auto pair = gen.generate("example.com");
    EXPECT_TRUE(pair.certPem.empty());
    EXPECT_TRUE(pair.keyPem.empty());
}

#include <gtest/gtest.h>
#include "proxy/CertCache.hpp"
#include "proxy/CertGenerator.hpp"
#include "proxy/SslInit.hpp"
#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

class CertCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        testCaDir_ = (fs::temp_directory_path() / "burptui_cert_cache_test").string();
        fs::remove_all(testCaDir_);
        ASSERT_TRUE(BurpTUI::SslInit::instance().initialize(testCaDir_));
    }

    void TearDown() override {
        fs::remove_all(testCaDir_);
    }

    std::string testCaDir_;
};

TEST_F(CertCacheTest, CachesCertificateAndSslContext) {
    BurpTUI::CertGenerator gen;
    BurpTUI::CertCache cache(gen, 10);

    EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.contains("example.com"));

    // First call: cache miss, generates and caches
    auto entry1 = cache.getEntry("example.com");
    EXPECT_FALSE(entry1.certKeyPair.certPem.empty());
    EXPECT_FALSE(entry1.certKeyPair.keyPem.empty());
    ASSERT_NE(entry1.sslContext, nullptr);
    EXPECT_NE(entry1.sslContext->native_handle(), nullptr);

    EXPECT_EQ(cache.size(), 1);
    EXPECT_TRUE(cache.contains("example.com"));

    // Second call: cache hit, returns same certificate and ssl context
    auto entry2 = cache.getEntry("example.com");
    EXPECT_EQ(entry1.certKeyPair.certPem, entry2.certKeyPair.certPem);
    EXPECT_EQ(entry1.certKeyPair.keyPem, entry2.certKeyPair.keyPem);
    EXPECT_EQ(entry1.sslContext, entry2.sslContext);

    // Convenience accessors
    auto keyPair = cache.get("example.com");
    EXPECT_EQ(keyPair.certPem, entry1.certKeyPair.certPem);

    auto ctx = cache.getContext("example.com");
    EXPECT_EQ(ctx, entry1.sslContext);
}

TEST_F(CertCacheTest, EvictsOldestWhenMaxCapacityReached) {
    BurpTUI::CertGenerator gen;
    BurpTUI::CertCache cache(gen, 2); // Max capacity 2

    auto entryA = cache.getEntry("site-a.com");
    auto entryB = cache.getEntry("site-b.com");

    EXPECT_EQ(cache.size(), 2);
    EXPECT_TRUE(cache.contains("site-a.com"));
    EXPECT_TRUE(cache.contains("site-b.com"));

    // Adding site-c.com should evict site-a.com (oldest entry)
    auto entryC = cache.getEntry("site-c.com");

    EXPECT_EQ(cache.size(), 2);
    EXPECT_FALSE(cache.contains("site-a.com"));
    EXPECT_TRUE(cache.contains("site-b.com"));
    EXPECT_TRUE(cache.contains("site-c.com"));
}

TEST_F(CertCacheTest, LruTouchPromotesEntry) {
    BurpTUI::CertGenerator gen;
    BurpTUI::CertCache cache(gen, 2);

    cache.getEntry("site-a.com");
    cache.getEntry("site-b.com");

    // Touch site-a to make site-b the least recently used
    cache.touch("site-a.com");

    // Adding site-c should now evict site-b
    cache.getEntry("site-c.com");

    EXPECT_TRUE(cache.contains("site-a.com"));
    EXPECT_FALSE(cache.contains("site-b.com"));
    EXPECT_TRUE(cache.contains("site-c.com"));
}

TEST_F(CertCacheTest, ClearRemovesAllEntries) {
    BurpTUI::CertGenerator gen;
    BurpTUI::CertCache cache(gen, 5);

    cache.getEntry("site-1.com");
    cache.getEntry("site-2.com");
    EXPECT_EQ(cache.size(), 2);

    cache.clear();
    EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.contains("site-1.com"));
    EXPECT_FALSE(cache.contains("site-2.com"));
}

TEST_F(CertCacheTest, ConcurrentAccessIsThreadSafe) {
    BurpTUI::CertGenerator gen;
    BurpTUI::CertCache cache(gen, 50);

    constexpr int numThreads = 8;
    constexpr int iterations = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < iterations; ++i) {
                std::string host = "thread-" + std::to_string(t % 4) + ".test.com";
                auto ctx = cache.getContext(host);
                EXPECT_NE(ctx, nullptr);
                EXPECT_TRUE(cache.contains(host));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_GT(cache.size(), 0);
}

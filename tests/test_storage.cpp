#include <gtest/gtest.h>
#include "storage/MemoryStore.hpp"

TEST(MemoryStore, SaveAndList) {
    BurpTUI::MemoryStore store;
    BurpTUI::ProxyEntry  entry;
    entry.method = "GET";
    entry.host   = "example.com";
    entry.url    = "/";
    store.save(entry);
    EXPECT_EQ(store.list().size(), 1u);
}

TEST(MemoryStore, Clear) {
    BurpTUI::MemoryStore store;
    BurpTUI::ProxyEntry  entry;
    store.save(entry);
    store.clear();
    EXPECT_TRUE(store.list().empty());
}

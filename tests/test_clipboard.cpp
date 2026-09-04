#include <gtest/gtest.h>
#include "util/Clipboard.hpp"

using namespace BurpTUI;

TEST(ClipboardTest, BasicCopy) {
    std::string sample = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    // Should return true or false cleanly without crashing
    bool res = Clipboard::copy(sample);
    (void)res;
    EXPECT_FALSE(Clipboard::copy("")); // empty text returns false
}

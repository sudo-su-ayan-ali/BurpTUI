#include <gtest/gtest.h>
#include "util/Encoding.hpp"

TEST(Encoding, Base64RoundTrip) {
    const std::string input = "Hello, BurpTUI!";
    EXPECT_EQ(BurpTUI::Encoding::base64Decode(
                  BurpTUI::Encoding::base64Encode(input)), input);
}

TEST(Encoding, UrlRoundTrip) {
    const std::string input = "foo bar+baz=qux&x=1";
    EXPECT_EQ(BurpTUI::Encoding::urlDecode(
                  BurpTUI::Encoding::urlEncode(input)), input);
}

TEST(Encoding, HexRoundTrip) {
    const std::string input = "deadbeef";
    EXPECT_EQ(BurpTUI::Encoding::hexDecode(
                  BurpTUI::Encoding::hexEncode(input)), input);
}

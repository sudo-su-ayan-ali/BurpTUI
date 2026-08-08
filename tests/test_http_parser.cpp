#include <gtest/gtest.h>
#include "http/HttpParser.hpp"

TEST(HttpParser, DefaultConstruction) {
    BurpTUI::HttpParser parser;
    EXPECT_FALSE(parser.feedRequest("GET / HTTP/1.1\r\n\r\n"));
    EXPECT_EQ(parser.takeRequest(), std::nullopt);
}

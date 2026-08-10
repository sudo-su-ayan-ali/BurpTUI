#include <gtest/gtest.h>
#include "http/HttpParser.hpp"

TEST(HttpParser, ParseSimpleGetRequest) {
    BurpTUI::HttpParser parser;
    EXPECT_TRUE(parser.feedRequest("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"));
    auto req = parser.takeRequest();
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->method, "GET");
    EXPECT_EQ(req->url, "/");
    EXPECT_EQ(req->version, "HTTP/1.1");
    EXPECT_EQ(req->header("Host"), "example.com");
    EXPECT_TRUE(req->body.empty());
}

TEST(HttpParser, ParsePostRequestWithBody) {
    BurpTUI::HttpParser parser;
    std::string raw = "POST /api/login HTTP/1.1\r\n"
                      "Host: api.example.com\r\n"
                      "Content-Length: 27\r\n"
                      "Content-Type: application/json\r\n"
                      "\r\n"
                      "{\"username\":\"admin\",\"pw\":1}";
    EXPECT_TRUE(parser.feedRequest(raw));
    auto req = parser.takeRequest();
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->method, "POST");
    EXPECT_EQ(req->url, "/api/login");
    EXPECT_EQ(req->header("Content-Type"), "application/json");
    EXPECT_EQ(req->body, "{\"username\":\"admin\",\"pw\":1}");
}

TEST(HttpParser, ParseSimpleResponse) {
    BurpTUI::HttpParser parser;
    std::string raw = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 13\r\n"
                      "\r\n"
                      "Hello, World!";
    EXPECT_TRUE(parser.feedResponse(raw));
    auto res = parser.takeResponse();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->statusCode, 200);
    EXPECT_EQ(res->statusText, "OK");
    EXPECT_EQ(res->version, "HTTP/1.1");
    EXPECT_EQ(res->header("Content-Type"), "text/html");
    EXPECT_EQ(res->body, "Hello, World!");
}

TEST(HttpParser, ResetAllowsReParsing) {
    BurpTUI::HttpParser parser;
    EXPECT_TRUE(parser.feedRequest("GET /first HTTP/1.1\r\nHost: a.com\r\n\r\n"));
    auto req1 = parser.takeRequest();
    ASSERT_TRUE(req1.has_value());
    EXPECT_EQ(req1->url, "/first");

    // After takeRequest resets, parse a new request
    EXPECT_TRUE(parser.feedRequest("GET /second HTTP/1.1\r\nHost: b.com\r\n\r\n"));
    auto req2 = parser.takeRequest();
    ASSERT_TRUE(req2.has_value());
    EXPECT_EQ(req2->url, "/second");
}

TEST(HttpParser, IncrementalFeeding) {
    BurpTUI::HttpParser parser;
    EXPECT_FALSE(parser.feedRequest("GET / HTTP/1.1\r\n"));
    EXPECT_FALSE(parser.feedRequest("Host: example.com\r\n"));
    EXPECT_TRUE(parser.feedRequest("\r\n"));
    auto req = parser.takeRequest();
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->method, "GET");
    EXPECT_EQ(req->header("Host"), "example.com");
}

TEST(HttpParser, Parse404Response) {
    BurpTUI::HttpParser parser;
    std::string raw = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Length: 9\r\n"
                      "\r\n"
                      "Not Found";
    EXPECT_TRUE(parser.feedResponse(raw));
    auto res = parser.takeResponse();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->statusCode, 404);
    EXPECT_EQ(res->statusText, "Not Found");
    EXPECT_EQ(res->body, "Not Found");
}

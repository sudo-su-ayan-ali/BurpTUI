#include <gtest/gtest.h>
#include "proxy/InterceptManager.hpp"
#include "http/HttpRequest.hpp"

using namespace BurpTUI;

TEST(InterceptManagerTest, InitialStateAndToggle) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(false);
    EXPECT_FALSE(im.isInterceptEnabled());

    im.setInterceptEnabled(true);
    EXPECT_TRUE(im.isInterceptEnabled());

    im.setInterceptEnabled(false);
    EXPECT_FALSE(im.isInterceptEnabled());
}

TEST(InterceptManagerTest, InterceptAndForward) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(true);

    auto req = std::make_shared<HttpRequest>();
    req->method = "GET";
    req->url = "/test";
    req->version = "HTTP/1.1";
    req->headers.push_back({"Host", "example.com"});

    InterceptAction decision = InterceptAction::Drop;
    bool callbackFired = false;

    im.interceptRequest(101, false, "example.com", 80, req, [&](InterceptAction act) {
        decision = act;
        callbackFired = true;
    });

    EXPECT_TRUE(im.hasPending());
    EXPECT_EQ(im.pendingCount(), 1u);

    auto peeked = im.peekCurrent();
    ASSERT_NE(peeked, nullptr);
    EXPECT_EQ(peeked->id, 101);
    EXPECT_EQ(peeked->host, "example.com");
    EXPECT_EQ(peeked->request->method, "GET");

    EXPECT_TRUE(im.forwardCurrent());
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(decision, InterceptAction::Forward);

    EXPECT_FALSE(im.hasPending());
    EXPECT_EQ(im.pendingCount(), 0u);

    im.setInterceptEnabled(false);
}

TEST(InterceptManagerTest, InterceptAndDrop) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(true);

    auto req = std::make_shared<HttpRequest>();
    req->method = "POST";
    req->url = "/login";
    req->version = "HTTP/1.1";

    InterceptAction decision = InterceptAction::Forward;
    bool callbackFired = false;

    im.interceptRequest(102, true, "secure.example.com", 443, req, [&](InterceptAction act) {
        decision = act;
        callbackFired = true;
    });

    EXPECT_TRUE(im.hasPending());
    EXPECT_TRUE(im.dropCurrent());
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(decision, InterceptAction::Drop);
    EXPECT_FALSE(im.hasPending());

    im.setInterceptEnabled(false);
}

TEST(InterceptManagerTest, DisableAutoForwardsAll) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(true);

    int forwardedCount = 0;
    for (int i = 1; i <= 3; ++i) {
        auto req = std::make_shared<HttpRequest>();
        req->method = "GET";
        im.interceptRequest(200 + i, false, "example.com", 80, req, [&](InterceptAction act) {
            if (act == InterceptAction::Forward) {
                forwardedCount++;
            }
        });
    }

    EXPECT_EQ(im.pendingCount(), 3u);
    // Disabling intercept automatically forwards all pending requests
    im.setInterceptEnabled(false);
    EXPECT_EQ(forwardedCount, 3);
    EXPECT_EQ(im.pendingCount(), 0u);
}

TEST(InterceptManagerTest, RemoveById) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(true);

    auto req = std::make_shared<HttpRequest>();
    im.interceptRequest(301, false, "example.com", 80, req, [](InterceptAction) {});
    im.interceptRequest(302, false, "example.com", 80, req, [](InterceptAction) {});

    EXPECT_EQ(im.pendingCount(), 2u);
    im.removeById(301);
    EXPECT_EQ(im.pendingCount(), 1u);
    EXPECT_EQ(im.peekCurrent()->id, 302);

    im.setInterceptEnabled(false);
}

TEST(InterceptManagerTest, NotifyCallbackFires) {
    auto& im = InterceptManager::instance();
    im.setInterceptEnabled(false);

    int notifyCount = 0;
    im.setNotifyCallback([&]() {
        notifyCount++;
    });

    im.setInterceptEnabled(true);
    EXPECT_GE(notifyCount, 1);

    auto req = std::make_shared<HttpRequest>();
    im.interceptRequest(401, false, "example.com", 80, req, [](InterceptAction) {});
    EXPECT_GE(notifyCount, 2);

    im.forwardCurrent();
    EXPECT_GE(notifyCount, 3);

    im.setInterceptEnabled(false);
    im.setNotifyCallback(nullptr);
}


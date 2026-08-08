#include <gtest/gtest.h>
#include "util/EventQueue.hpp"
#include <thread>

TEST(EventQueue, BasicPushPop) {
    BurpTUI::EventQueue<int> q;
    q.push(42);
    EXPECT_EQ(q.pop(), 42);
}

TEST(EventQueue, ThreadedPushPop) {
    BurpTUI::EventQueue<std::string> q;
    std::thread producer([&]{ q.push("hello"); });
    auto val = q.pop();
    producer.join();
    EXPECT_EQ(val, "hello");
}

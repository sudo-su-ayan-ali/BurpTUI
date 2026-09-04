#include <gtest/gtest.h>
#include "http/HttpRequest.hpp"
#include "tui/TextEditor.hpp"
#include "tui/RepeaterManager.hpp"
#include <ftxui/component/event.hpp>

using namespace BurpTUI;

TEST(HttpRawParseTest, BasicRequest) {
    std::string raw = 
        "POST /api/v1/test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"hello\":\"world\"}";

    HttpRequest req = ParseRawHttpRequest(raw);
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.url, "/api/v1/test");
    EXPECT_EQ(req.version, "HTTP/1.1");
    EXPECT_EQ(req.header("Host"), "example.com");
    EXPECT_EQ(req.header("Content-Type"), "application/json");
    EXPECT_EQ(req.body, "{\"hello\":\"world\"}");
    EXPECT_EQ(req.serialize(), raw);
}

TEST(TextEditorTest, BasicEditing) {
    auto editor = MakeTextEditor("Test");
    editor->SetText("Hello");
    EXPECT_EQ(editor->GetText(), "Hello");

    editor->TakeFocus();
    // Navigate to end
    editor->OnEvent(ftxui::Event::End);
    // Type ' World'
    for (char c : std::string(" World")) {
        editor->OnEvent(ftxui::Event::Character(c));
    }
    EXPECT_EQ(editor->GetText(), "Hello World");

    // Add newline
    editor->OnEvent(ftxui::Event::Return);
    for (char c : std::string("Line 2")) {
        editor->OnEvent(ftxui::Event::Character(c));
    }
    EXPECT_EQ(editor->GetText(), "Hello World\r\nLine 2");
    EXPECT_EQ(editor->GetTotalLines(), 2);
}

TEST(RepeaterManagerTest, StateAndTarget) {
    auto& mgr = RepeaterManager::instance();
    mgr.setRequest("example.com", "8443", true, "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n");

    EXPECT_EQ(mgr.getHost(), "example.com");
    EXPECT_EQ(mgr.getPort(), "8443");
    EXPECT_TRUE(mgr.isHttps());
    EXPECT_EQ(mgr.getRawRequest(), "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n");
}

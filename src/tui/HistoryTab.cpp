#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace BurpTUI {

using namespace ftxui;

struct MockHistoryEntry {
    std::string method;
    std::string status;
    std::string host;
    std::string url;
    std::string requestDetail;
    std::string responseDetail;
};

ftxui::Component MakeHistoryTab() {
    auto entries = std::make_shared<std::vector<MockHistoryEntry>>(std::vector<MockHistoryEntry>{
        { "GET",  "200 OK", "api.github.com",   "/users/octocat",
          "GET /users/octocat HTTP/1.1\nHost: api.github.com\nUser-Agent: curl/7.68.0",
          "HTTP/1.1 200 OK\nContent-Type: application/json\n\n{\n  \"login\": \"octocat\",\n  \"id\": 5832347\n}" },
        { "POST", "201 OK", "api.example.com",  "/v1/auth/login",
          "POST /v1/auth/login HTTP/1.1\nHost: api.example.com\nContent-Length: 25\n\n{\"username\":\"admin\"}",
          "HTTP/1.1 201 Created\nSet-Cookie: session=xyz123\nContent-Length: 18\n\n{\"status\":\"success\"}" },
        { "GET",  "404 ERR", "static.test.org", "/images/logo.png",
          "GET /images/logo.png HTTP/1.1\nHost: static.test.org\nAccept: image/*",
          "HTTP/1.1 404 Not Found\nContent-Length: 9\n\nNot Found" }
    });

    auto selectedIndex = std::make_shared<int>(0);

    std::vector<std::string> entryLabels;
    for (const auto& e : *entries) {
        entryLabels.push_back(" " + e.method + "  [" + e.status + "]  " + e.host + e.url);
    }

    auto menuComp = Menu(&entryLabels, selectedIndex.get());

    return Renderer(menuComp, [=] {
        int idx = *selectedIndex;
        if (idx < 0 || idx >= static_cast<int>(entries->size())) {
            idx = 0;
        }

        const auto& active = (*entries)[idx];

        return vbox(Elements{
            Widgets::Panel("Captured Requests History List", menuComp->Render() | frame) | size(HEIGHT, EQUAL, 8),
            hbox(Elements{
                Widgets::Panel("Request Inspectors", paragraph(active.requestDetail) | flex) | flex,
                Widgets::Panel("Response Inspectors", paragraph(active.responseDetail) | flex) | flex,
            }) | flex,
            Widgets::StatusBar("History Inspection Active", "Total captured: " + std::to_string(entries->size())),
        });
    });
}

} // namespace BurpTUI

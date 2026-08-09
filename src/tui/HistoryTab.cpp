#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include "http/HttpTransaction.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace BurpTUI {

using namespace ftxui;

std::string FormatHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::ostringstream oss;
    for (const auto& h : headers) {
        oss << h.first << ": " << h.second << "\n";
    }
    return oss.str();
}

class DetailPaneBase : public ftxui::ComponentBase {
public:
    DetailPaneBase(std::function<ftxui::Element(bool)> render) : render_(render) {}
    bool Focusable() const override { return true; }
    ftxui::Element Render() override { return render_(Focused()); }
private:
    std::function<ftxui::Element(bool)> render_;
};

ftxui::Component DetailPaneComponent(std::function<ftxui::Element(bool)> render) {
    return std::make_shared<DetailPaneBase>(render);
}

ftxui::Component MakeHistoryTab() {
    auto entries = std::make_shared<std::vector<HttpTransaction>>();

    // Populate with synthetic dummy data
    {
        HttpTransaction t1;
        t1.id = 1;
        t1.host = "api.github.com";
        t1.port = 443;
        t1.is_https = true;
        t1.request = std::make_shared<HttpRequest>();
        t1.request->method = "GET";
        t1.request->url = "/users/octocat";
        t1.request->version = "HTTP/1.1";
        t1.request->headers = {{"Host", "api.github.com"}, {"User-Agent", "curl/7.68.0"}};
        t1.request->body = "";
        
        t1.response = std::make_shared<HttpResponse>();
        t1.response->version = "HTTP/1.1";
        t1.response->statusCode = 200;
        t1.response->statusText = "OK";
        t1.response->headers = {{"Content-Type", "application/json"}};
        t1.response->body = "{\n  \"login\": \"octocat\",\n  \"id\": 5832347\n}";
        
        entries->push_back(t1);
    }
    
    {
        HttpTransaction t2;
        t2.id = 2;
        t2.host = "api.example.com";
        t2.port = 443;
        t2.is_https = true;
        t2.request = std::make_shared<HttpRequest>();
        t2.request->method = "POST";
        t2.request->url = "/v1/auth/login";
        t2.request->version = "HTTP/1.1";
        t2.request->headers = {{"Host", "api.example.com"}, {"Content-Length", "25"}};
        t2.request->body = "{\"username\":\"admin\"}";
        
        t2.response = std::make_shared<HttpResponse>();
        t2.response->version = "HTTP/1.1";
        t2.response->statusCode = 201;
        t2.response->statusText = "Created";
        t2.response->headers = {{"Set-Cookie", "session=xyz123"}, {"Content-Length", "18"}};
        t2.response->body = "{\"status\":\"success\"}";
        
        entries->push_back(t2);
    }

    {
        HttpTransaction t3;
        t3.id = 3;
        t3.host = "static.test.org";
        t3.port = 80;
        t3.is_https = false;
        t3.request = std::make_shared<HttpRequest>();
        t3.request->method = "GET";
        t3.request->url = "/images/logo.png";
        t3.request->version = "HTTP/1.1";
        t3.request->headers = {{"Host", "static.test.org"}, {"Accept", "image/*"}};
        t3.request->body = "";
        
        t3.response = std::make_shared<HttpResponse>();
        t3.response->version = "HTTP/1.1";
        t3.response->statusCode = 404;
        t3.response->statusText = "Not Found";
        t3.response->headers = {{"Content-Length", "9"}};
        t3.response->body = "Not Found";
        
        entries->push_back(t3);
    }

    auto selectedIndex = std::make_shared<int>(0);

    auto entryLabels = std::make_shared<std::vector<std::string>>();
    for (const auto& e : *entries) {
        std::string statusStr = std::to_string(e.response->statusCode);
        entryLabels->push_back(" " + e.request->method + "  [" + statusStr + "]  " + e.host + e.request->url);
    }

    auto menuComp = Menu(entryLabels.get(), selectedIndex.get());

    auto detailComp = DetailPaneComponent([=](bool focused) {
        int idx = *selectedIndex;
        if (idx < 0 || idx >= static_cast<int>(entries->size())) {
            idx = 0;
        }

        const auto& active = (*entries)[idx];
        
        std::ostringstream reqDetail;
        reqDetail << active.request->method << " " << active.request->url << " " << active.request->version << "\n";
        reqDetail << FormatHeaders(active.request->headers) << "\n";
        reqDetail << active.request->body;
        
        std::ostringstream resDetail;
        resDetail << active.response->version << " " << active.response->statusCode << " " << active.response->statusText << "\n";
        resDetail << FormatHeaders(active.response->headers) << "\n";
        resDetail << active.response->body;

        auto reqPanel = Widgets::Panel("Request", paragraph(reqDetail.str()) | vscroll_indicator | yframe);
        auto resPanel = Widgets::Panel("Response", paragraph(resDetail.str()) | vscroll_indicator | yframe);
        
        if (focused) {
            return vbox(Elements{
                reqPanel | flex,
                resPanel | flex,
            }) | borderLight | color(Color::Green);
        }

        return vbox(Elements{
            reqPanel | flex,
            resPanel | flex,
        }) | borderEmpty;
    });

    auto layout = Container::Horizontal({menuComp, detailComp});

    layout = CatchEvent(layout, [=](Event event) {
        if (event == Event::Character('h')) {
            return layout->OnEvent(Event::ArrowLeft);
        }
        if (event == Event::Character('l')) {
            return layout->OnEvent(Event::ArrowRight);
        }
        // Menu natively supports j/k and Up/Down for scrolling.
        return false;
    });

    return Renderer(layout, [=] {
        return vbox(Elements{
            hbox(Elements{
                Widgets::Panel("Captured Requests", menuComp->Render() | vscroll_indicator | frame) | size(WIDTH, LESS_THAN, 40),
                detailComp->Render() | flex,
            }) | flex,
            Widgets::StatusBar("History Inspection Active", "Total captured: " + std::to_string(entries->size())),
        });
    });
}

} // namespace BurpTUI

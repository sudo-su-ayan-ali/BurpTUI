#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace BurpTUI {

using namespace ftxui;

// Keep the existing FormatHeaders helper
std::string FormatHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::ostringstream oss;
    for (const auto& h : headers)
        oss << h.first << ": " << h.second << "\n";
    return oss.str();
}

// Keep the existing DetailPaneBase helper
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

ftxui::Component MakeHistoryTab(
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue,
    ftxui::ScreenInteractive* screen)
{
    (void)screen;
    auto entries = std::make_shared<std::vector<HttpTransaction>>();
    auto entryLabels = std::make_shared<std::vector<std::string>>();
    auto selectedIndex = std::make_shared<int>(0);
    auto queue = txQueue;

    // Populate with dummy HTTP traffic for Phase 1 Step 3
    for (int i = 1; i <= 5; ++i) {
        HttpTransaction tx;
        tx.id = i;
        tx.host = "example.com";
        tx.port = 80;
        tx.is_https = false;
        
        tx.request = std::make_shared<HttpRequest>();
        tx.request->method = (i % 2 == 0) ? "POST" : "GET";
        tx.request->url = "/api/v1/resource/" + std::to_string(i);
        tx.request->version = "HTTP/1.1";
        tx.request->headers = {{"Host", "example.com"}, {"User-Agent", "BurpTUI-Agent/1.0"}};
        tx.request->body = (i % 2 == 0) ? "{\"action\":\"update\",\"value\":" + std::to_string(i) + "}" : "";
        
        tx.response = std::make_shared<HttpResponse>();
        tx.response->version = "HTTP/1.1";
        tx.response->statusCode = (i == 4) ? 404 : 200;
        tx.response->statusText = (i == 4) ? "Not Found" : "OK";
        tx.response->headers = {{"Content-Type", "application/json"}};
        tx.response->body = (i == 4) ? "{\"error\":\"not found\"}" : "{\"status\":\"success\"}";
        
        std::string statusStr = std::to_string(tx.response->statusCode);
        std::string label = " " + tx.request->method + "  [" + statusStr + "]  " + tx.host + tx.request->url;
        
        entryLabels->push_back(std::move(label));
        entries->push_back(std::move(tx));
    }


    auto menuComp = Menu(entryLabels.get(), selectedIndex.get());

    auto detailComp = DetailPaneComponent([=](bool focused) {
        if (entries->empty()) {
            return text("  Waiting for traffic... Configure your browser to use proxy.") | dim | center;
        }

        int idx = *selectedIndex;
        if (idx < 0 || idx >= static_cast<int>(entries->size())) idx = 0;

        const auto& active = (*entries)[idx];

        std::ostringstream reqDetail;
        if (active.request) {
            reqDetail << active.request->method << " " << active.request->url << " " << active.request->version << "\n";
            reqDetail << FormatHeaders(active.request->headers) << "\n";
            reqDetail << active.request->body;
        }

        std::ostringstream resDetail;
        if (active.response) {
            resDetail << active.response->version << " " << active.response->statusCode << " " << active.response->statusText << "\n";
            resDetail << FormatHeaders(active.response->headers) << "\n";
            resDetail << active.response->body;
        } else {
            resDetail << "(Response pending...)";
        }

        auto reqPanel = Widgets::Panel("Request", paragraph(reqDetail.str()) | vscroll_indicator | yframe);
        auto resPanel = Widgets::Panel("Response", paragraph(resDetail.str()) | vscroll_indicator | yframe);

        auto content = vbox(Elements{reqPanel | flex, resPanel | flex});
        return focused ? (content | borderLight | color(Color::Green)) : (content | borderEmpty);
    });

    auto layout = Container::Horizontal({menuComp, detailComp});

    layout = CatchEvent(layout, [=](Event event) {
        if (event == Event::Character('h')) return layout->OnEvent(Event::ArrowLeft);
        if (event == Event::Character('l')) return layout->OnEvent(Event::ArrowRight);
        return false;
    });

    return Renderer(layout, [=] {
        // Drain new transactions from the queue on every render
        auto newItems = queue->tryPopAll();
        for (auto& tx : newItems) {
            std::string statusStr = tx.response ? std::to_string(tx.response->statusCode) : "???";
            std::string label = " " + (tx.request ? tx.request->method : "?") 
                              + "  [" + statusStr + "]  " + tx.host 
                              + (tx.request ? tx.request->url : "");
            entryLabels->push_back(std::move(label));
            entries->push_back(std::move(tx));
        }

        return vbox(Elements{
            hbox(Elements{
                Widgets::Panel("Captured Requests", menuComp->Render() | vscroll_indicator | frame) | size(WIDTH, LESS_THAN, 45),
                detailComp->Render() | flex,
            }) | flex,
            Widgets::StatusBar("History Inspection Active", "Total captured: " + std::to_string(entries->size())),
        });
    });
}

} // namespace BurpTUI

#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace BurpTUI {

using namespace ftxui;

namespace {

// Clean text: strip \r and replace dangerous ANSI control characters that break terminal layout
std::string SanitizeText(std::string_view input, std::size_t maxLen = 4096) {
    std::string out;
    out.reserve(std::min(input.size(), maxLen));
    std::size_t count = 0;
    for (unsigned char c : input) {
        if (count >= maxLen) break;
        if (c == '\r') continue;
        if (c == '\n' || c == '\t' || (c >= 32 && c < 127)) {
            out += static_cast<char>(c);
            count++;
        } else if (c >= 128) {
            out += static_cast<char>(c);
            count++;
        } else {
            out += '.';
            count++;
        }
    }
    return out;
}

// Detect binary data (e.g. gzip compressed, images, executables, fonts)
bool IsBinaryData(std::string_view data, std::string_view contentType) {
    if (contentType.find("image/") != std::string_view::npos ||
        contentType.find("audio/") != std::string_view::npos ||
        contentType.find("video/") != std::string_view::npos ||
        contentType.find("font/") != std::string_view::npos ||
        contentType.find("application/octet-stream") != std::string_view::npos ||
        contentType.find("application/zip") != std::string_view::npos ||
        contentType.find("application/gzip") != std::string_view::npos ||
        contentType.find("application/pdf") != std::string_view::npos) {
        return true;
    }

    std::size_t sample = std::min<std::size_t>(data.size(), 512);
    if (sample == 0) return false;
    std::size_t nonPrintable = 0;
    for (std::size_t i = 0; i < sample; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0) return true; // Null byte strongly indicates binary
        if (c < 9 || (c > 13 && c < 32)) {
            nonPrintable++;
        }
    }
    return (nonPrintable * 100 / sample) > 8;
}

// Format binary payload as a readable hex dump preview
std::string FormatHexDump(std::string_view data, std::size_t maxBytes = 256) {
    std::ostringstream oss;
    std::size_t limit = std::min(data.size(), maxBytes);
    for (std::size_t i = 0; i < limit; i += 16) {
        char offsetBuf[32];
        std::snprintf(offsetBuf, sizeof(offsetBuf), "%08zx: ", i);
        oss << offsetBuf;

        for (std::size_t j = 0; j < 16; ++j) {
            if (i + j < limit) {
                char byteBuf[4];
                std::snprintf(byteBuf, sizeof(byteBuf), "%02x ", static_cast<unsigned char>(data[i + j]));
                oss << byteBuf;
            } else {
                oss << "   ";
            }
            if (j == 7) oss << " ";
        }
        oss << " |";
        for (std::size_t j = 0; j < 16 && i + j < limit; ++j) {
            unsigned char c = static_cast<unsigned char>(data[i + j]);
            oss << (c >= 32 && c < 127 ? static_cast<char>(c) : '.');
        }
        oss << "|\n";
    }
    if (data.size() > limit) {
        oss << "[ ... " << (data.size() - limit) << " additional binary bytes omitted ... ]\n";
    }
    return oss.str();
}

Element FormatBodyElement(const std::string& body, const std::string& contentType) {
    if (body.empty()) {
        return text("(Empty body)") | dim;
    }
    if (IsBinaryData(body, contentType)) {
        Elements binaryElements;
        binaryElements.push_back(
            text("[ Binary Payload: " + std::to_string(body.size()) + " bytes | Content-Type: " + 
                 (contentType.empty() ? "unknown" : contentType) + " ]") | color(Color::YellowLight)
        );
        binaryElements.push_back(text(""));
        
        std::string hex = FormatHexDump(body, 256);
        std::istringstream stream(hex);
        std::string line;
        while (std::getline(stream, line)) {
            binaryElements.push_back(text(line) | color(Color::GrayLight));
        }
        return vbox(std::move(binaryElements));
    }

    // Text body: format line-by-line (preserves all line breaks and indentation)
    Elements textLines;
    constexpr std::size_t MAX_LINES = 250;
    std::istringstream stream(body);
    std::string line;
    std::size_t lineCount = 0;
    while (std::getline(stream, line) && lineCount < MAX_LINES) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        textLines.push_back(text(SanitizeText(line, 512)) | color(Color::White));
        lineCount++;
    }

    if (stream.good()) {
        textLines.push_back(text("\n[ ... Remaining lines truncated for TUI performance ... ]") | dim);
    }

    return vbox(std::move(textLines));
}

Element FormatHttpRequest(const HttpRequest& req, bool isHttps, const std::string& host) {
    Elements elements;
    // Method line: GET /path HTTP/1.1
    elements.push_back(hbox({
        text(req.method + " ") | bold | color(Color::Cyan),
        text(SanitizeText(req.url, 256) + " ") | bold | color(Color::White),
        text(req.version) | dim,
    }));
    elements.push_back(separatorLight());

    // Host & Scheme info
    elements.push_back(hbox({
        text("Host: ") | bold | color(Color::BlueLight),
        text(host + (isHttps ? " (HTTPS)" : " (HTTP)")) | color(Color::White),
    }));

    // Headers
    for (const auto& [k, v] : req.headers) {
        if (k == "Host") continue;
        elements.push_back(hbox({
            text(SanitizeText(k, 64) + ": ") | bold | color(Color::BlueLight),
            text(SanitizeText(v, 256)) | color(Color::White),
        }));
    }

    // Body
    if (!req.body.empty()) {
        elements.push_back(separatorLight());
        elements.push_back(text("--- Request Body (" + std::to_string(req.body.size()) + " bytes) ---") | dim);
        elements.push_back(FormatBodyElement(req.body, req.header("Content-Type")));
    }

    return vbox(std::move(elements));
}

Element FormatHttpResponse(const HttpResponse& res) {
    Elements elements;
    // Status line: HTTP/1.1 200 OK
    Color statusColor = (res.statusCode < 300) ? Color::Green :
                        (res.statusCode < 400) ? Color::Yellow : Color::Red;

    elements.push_back(hbox({
        text(res.version + " ") | dim,
        text(std::to_string(res.statusCode) + " " + SanitizeText(res.statusText, 64)) | bold | color(statusColor),
    }));
    elements.push_back(separatorLight());

    // Headers
    for (const auto& [k, v] : res.headers) {
        elements.push_back(hbox({
            text(SanitizeText(k, 64) + ": ") | bold | color(Color::BlueLight),
            text(SanitizeText(v, 256)) | color(Color::White),
        }));
    }

    // Body
    if (!res.body.empty()) {
        elements.push_back(separatorLight());
        elements.push_back(text("--- Response Body (" + std::to_string(res.body.size()) + " bytes) ---") | dim);
        elements.push_back(FormatBodyElement(res.body, res.header("Content-Type")));
    }

    return vbox(std::move(elements));
}

std::string BuildMenuLabel(const HttpTransaction& tx) {
    std::string idStr = "#" + std::to_string(tx.id);
    while (idStr.size() < 4) idStr += ' ';

    std::string method = tx.request ? tx.request->method : "?";
    while (method.size() < 7) method += ' ';

    std::string statusStr = tx.response ? std::to_string(tx.response->statusCode) : "???";
    while (statusStr.size() < 3) statusStr += ' ';

    std::string proto = tx.is_https ? "HTTPS" : "HTTP ";

    std::string host = tx.host;
    if (host.size() > 16) {
        host = host.substr(0, 13) + "...";
    }
    while (host.size() < 16) host += ' ';

    std::string path = tx.request ? tx.request->url : "/";
    if (path.size() > 22) {
        path = path.substr(0, 19) + "...";
    }

    return idStr + " " + method + " [" + statusStr + "] " + proto + " " + host + " " + path;
}

class DetailPaneBase : public ftxui::ComponentBase {
public:
    explicit DetailPaneBase(std::function<ftxui::Element(bool)> render) : render_(std::move(render)) {}
    bool Focusable() const override { return true; }
    ftxui::Element Render() override { return render_(Focused()); }
private:
    std::function<ftxui::Element(bool)> render_;
};

ftxui::Component DetailPaneComponent(std::function<ftxui::Element(bool)> render) {
    return std::make_shared<DetailPaneBase>(std::move(render));
}

} // anonymous namespace

ftxui::Component MakeHistoryTab(
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue,
    ftxui::ScreenInteractive* screen,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries)
{
    (void)screen;
    auto entries = sharedEntries ? sharedEntries : std::make_shared<std::vector<HttpTransaction>>();
    auto entryLabels = std::make_shared<std::vector<std::string>>();
    auto selectedIndex = std::make_shared<int>(0);
    auto queue = txQueue;

    // Pre-populate labels if entries already exist
    for (const auto& tx : *entries) {
        entryLabels->push_back(BuildMenuLabel(tx));
    }

    auto menuComp = Menu(entryLabels.get(), selectedIndex.get());

    auto detailComp = DetailPaneComponent([=](bool focused) {
        if (entries->empty()) {
            return vbox(Elements{
                filler(),
                text("  Waiting for traffic... Configure your browser proxy to 127.0.0.1:8080") | dim | center,
                text("  For HTTPS, open http://burp/cert in your browser to install the Root CA.") | dim | center,
                filler(),
            });
        }

        int idx = *selectedIndex;
        if (idx < 0 || idx >= static_cast<int>(entries->size())) idx = 0;

        const auto& active = (*entries)[idx];

        Element reqContent;
        if (active.request) {
            reqContent = FormatHttpRequest(*active.request, active.is_https, active.host);
        } else {
            reqContent = text("(No request details available)") | dim;
        }

        Element resContent;
        if (active.response) {
            resContent = FormatHttpResponse(*active.response);
        } else {
            resContent = text("(Response pending...)") | dim;
        }

        auto reqPanel = Widgets::Panel(
            "Request: " + (active.request ? active.request->method : "?") + " " + active.host + (active.is_https ? " (HTTPS)" : " (HTTP)"),
            vbox({reqContent}) | vscroll_indicator | yframe | flex
        );

        std::string statusTitle = "Response";
        if (active.response) {
            statusTitle += " [" + std::to_string(active.response->statusCode) + " " + active.response->statusText + "]";
        }
        auto resPanel = Widgets::Panel(
            statusTitle,
            vbox({resContent}) | vscroll_indicator | yframe | flex
        );

        auto content = vbox(Elements{reqPanel | flex, resPanel | flex});
        return focused ? (content | borderLight | color(Color::Green)) : (content | borderEmpty);
    });

    auto layout = Container::Horizontal({menuComp, detailComp});

    layout = CatchEvent(layout, [=](Event event) {
        if (event == Event::Custom) {
            auto newItems = queue->tryPopAll();
            for (auto& tx : newItems) {
                entryLabels->push_back(BuildMenuLabel(tx));
                entries->push_back(std::move(tx));
            }
            return false;
        }
        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (*selectedIndex + 1 < static_cast<int>(entries->size())) {
                (*selectedIndex)++;
            }
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (*selectedIndex > 0) {
                (*selectedIndex)--;
            }
            return true;
        }
        if (event == Event::Character('h')) return layout->OnEvent(Event::ArrowLeft);
        if (event == Event::Character('l')) return layout->OnEvent(Event::ArrowRight);
        return false;
    });

    return Renderer(layout, [=] {
        std::string statusRight = "Total Captured: " + std::to_string(entries->size()) + " | [j/k] Navigate | [Tab] Switch Tab";
        return vbox(Elements{
            hbox(Elements{
                Widgets::Panel("Captured Requests", menuComp->Render() | vscroll_indicator | yframe) | size(WIDTH, EQUAL, 54),
                detailComp->Render() | flex,
            }) | flex,
            Widgets::StatusBar("History Inspection Active", statusRight),
        });
    });
}

} // namespace BurpTUI

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
std::string SanitizeText(std::string_view input, std::size_t maxLen = 8192) {
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
        oss << "\n[ ... " << (data.size() - limit) << " additional binary bytes omitted ... ]\n";
    }
    return oss.str();
}

std::string FormatBody(std::string_view body, std::string_view contentType) {
    if (body.empty()) return "(Empty body)";
    if (IsBinaryData(body, contentType)) {
        std::ostringstream oss;
        oss << "[ Binary Payload: " << body.size() << " bytes | Content-Type: "
            << (contentType.empty() ? "unknown" : contentType) << " ]\n\n";
        oss << FormatHexDump(body, 256);
        return oss.str();
    }

    constexpr std::size_t MAX_DISPLAY_CHARS = 8192;
    std::string sanitized = SanitizeText(body, MAX_DISPLAY_CHARS);
    if (body.size() > MAX_DISPLAY_CHARS) {
        sanitized += "\n\n[ ... Body truncated for TUI display (showing " +
                     std::to_string(MAX_DISPLAY_CHARS) + " of " + std::to_string(body.size()) + " bytes) ... ]";
    }
    return sanitized;
}

std::string FormatHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::ostringstream oss;
    for (const auto& h : headers) {
        std::string key = SanitizeText(h.first, 128);
        std::string val = SanitizeText(h.second, 512);
        oss << key << ": " << val << "\n";
    }
    return oss.str();
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

        // Format Request Details
        std::ostringstream reqDetail;
        if (active.request) {
            reqDetail << active.request->method << " " << active.request->url << " " << active.request->version << "\n";
            reqDetail << FormatHeaders(active.request->headers) << "\n";
            std::string reqContentType = active.request->header("Content-Type");
            reqDetail << FormatBody(active.request->body, reqContentType);
        } else {
            reqDetail << "(No request details available)";
        }

        // Format Response Details
        std::ostringstream resDetail;
        if (active.response) {
            resDetail << active.response->version << " " << active.response->statusCode << " " << active.response->statusText << "\n";
            resDetail << FormatHeaders(active.response->headers) << "\n";
            std::string resContentType = active.response->header("Content-Type");
            resDetail << FormatBody(active.response->body, resContentType);
        } else {
            resDetail << "(Response pending...)";
        }

        auto reqPanel = Widgets::Panel(
            "Request: " + (active.request ? active.request->method : "?") + " " + active.host + (active.is_https ? " (HTTPS)" : " (HTTP)"),
            paragraph(reqDetail.str()) | vscroll_indicator | yframe
        );

        std::string statusTitle = "Response";
        if (active.response) {
            statusTitle += " [" + std::to_string(active.response->statusCode) + " " + active.response->statusText + "]";
        }
        auto resPanel = Widgets::Panel(
            statusTitle,
            paragraph(resDetail.str()) | vscroll_indicator | yframe
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

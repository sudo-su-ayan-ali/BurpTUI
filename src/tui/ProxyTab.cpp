#include "tui/ProxyTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace BurpTUI {

using namespace ftxui;

namespace {

std::string SanitizePreview(std::string_view input, std::size_t maxLen = 2048) {
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

} // anonymous namespace

ftxui::Component MakeProxyTab(
    const Config& cfg,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries)
{
    struct State {
        bool interceptOn = false;
        std::string toggleLabel = " Intercept: OFF ";
        int forwardedCount = 0;
        int droppedCount = 0;
    };
    auto state = std::make_shared<State>();
    auto entries = sharedEntries;

    auto toggleButton = Button(&state->toggleLabel, [=] {
        state->interceptOn = !state->interceptOn;
        if (state->interceptOn) {
            state->toggleLabel = " Intercept: ON  ";
        } else {
            state->toggleLabel = " Intercept: OFF ";
        }
    });

    auto forwardButton = Button(" Forward ", [=] {
        state->forwardedCount++;
    });

    auto dropButton = Button(" Drop ", [=] {
        state->droppedCount++;
    });

    auto container = Container::Horizontal({
        toggleButton,
        forwardButton,
        dropButton,
    });

    return Renderer(container, [=] {
        auto btnToggle = toggleButton->Render();
        if (state->interceptOn) {
            btnToggle = btnToggle | color(Color::Red) | bold;
        } else {
            btnToggle = btnToggle | color(Color::Green) | bold;
        }

        auto btnForward = forwardButton->Render();
        auto btnDrop = dropButton->Render();

        Element mainPanelContent;

        if (entries && !entries->empty()) {
            const auto& latest = entries->back();

            std::string method = latest.request ? latest.request->method : "UNKNOWN";
            std::string url = latest.request ? latest.request->url : "/";
            std::string proto = latest.is_https ? "https://" : "http://";
            std::string fullUrl = proto + latest.host + url;

            std::string statusStr = latest.response ? (std::to_string(latest.response->statusCode) + " " + latest.response->statusText) : "Pending";

            std::ostringstream details;
            details << "Latest Live Transaction (#" << latest.id << "):\n";
            details << "--------------------------------------------------------\n";
            details << method << " " << fullUrl << " HTTP/1.1\n";
            details << "Response Status: " << statusStr << "\n\n";
            details << "--- Request Headers ---\n";
            if (latest.request) {
                for (const auto& [k, v] : latest.request->headers) {
                    details << k << ": " << SanitizePreview(v, 256) << "\n";
                }
                if (!latest.request->body.empty()) {
                    details << "\n--- Request Body (" << latest.request->body.size() << " bytes) ---\n";
                    details << SanitizePreview(latest.request->body, 1024) << "\n";
                }
            }

            if (latest.response && !latest.response->body.empty()) {
                details << "\n--- Response Body Preview (" << latest.response->body.size() << " bytes) ---\n";
                details << SanitizePreview(latest.response->body, 1024) << "\n";
            }

            mainPanelContent = vbox(Elements{
                hbox(Elements{
                    text("  ● LIVE STREAMING: ") | bold | color(Color::Green),
                    text(method + " " + latest.host + url) | bold,
                    filler(),
                    text("[" + statusStr + "]  ") | (latest.response && latest.response->statusCode < 400 ? color(Color::Green) : color(Color::Yellow)),
                }),
                separator(),
                paragraph(details.str()) | vscroll_indicator | yframe | flex,
            });
        } else {
            mainPanelContent = vbox(Elements{
                filler(),
                text("  ● Proxy Listener Active: " + cfg.listenHost + ":" + std::to_string(cfg.listenPort)) | bold | color(Color::Green) | center,
                text("") | center,
                text("Waiting for browser traffic to pass through...") | center,
                text("") | center,
                text("Quick Browser Configuration:") | bold | center,
                text("1. Set HTTP & HTTPS proxy in browser: " + cfg.listenHost + ":" + std::to_string(cfg.listenPort)) | center,
                text("2. Open http://burp in your browser to download the Root CA certificate") | color(Color::Cyan) | center,
                text("3. Trust the CA certificate in browser settings") | center,
                text("4. Browse any website — requests will stream live here and into [ History ]!") | color(Color::Green) | center,
                filler(),
            });
        }

        std::size_t totalCount = entries ? entries->size() : 0;

        return vbox(Elements{
            hbox(Elements{
                btnToggle,
                separator(),
                btnForward,
                separator(),
                btnDrop,
                filler(),
                text("● LISTENER: " + cfg.listenHost + ":" + std::to_string(cfg.listenPort)) | color(Color::Green) | bold,
            }) | size(HEIGHT, EQUAL, 3),
            separator(),
            Widgets::Panel("Live Intercept & Traffic Stream", mainPanelContent) | flex,
            Widgets::StatusBar(
                state->interceptOn ? "INTERCEPT MODE ACTIVE" : "PASS-THROUGH (STREAMING LIVE)", 
                "Total Captured: " + std::to_string(totalCount) + "  |  Switch to History: [Tab]"
            ),
        });
    });
}

} // namespace BurpTUI

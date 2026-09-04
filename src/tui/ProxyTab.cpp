#include "tui/ProxyTab.hpp"
#include "tui/Widgets.hpp"
#include "tui/HttpFormatter.hpp"
#include "tui/ScrollableViewer.hpp"
#include "tui/TextEditor.hpp"
#include "tui/RepeaterManager.hpp"
#include "proxy/InterceptManager.hpp"
#include "util/Clipboard.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>

namespace BurpTUI {

using namespace ftxui;

Component MakeProxyTab(
    const Config& cfg,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries)
{
    struct State {
        std::string toggleLabel = " Intercept: OFF ";
        int lastDisplayedTxId = -1;
        int lastInterceptId = -1;
        int viewMode = 1; // 0 = Intercept, 1 = Live
    };
    auto state = std::make_shared<State>();
    auto entries = sharedEntries;

    auto interceptEditor = MakeTextEditor("Intercepted Request (Editable)");
    auto liveReqViewer    = MakeScrollableViewer("Latest Request");
    auto liveResViewer    = MakeScrollableViewer("Latest Response");

    auto toggleButton = Button(&state->toggleLabel, [=] {
        bool currentlyOn = InterceptManager::instance().isInterceptEnabled();
        InterceptManager::instance().setInterceptEnabled(!currentlyOn);
        if (!currentlyOn) {
            state->toggleLabel = " Intercept: ON  ";
            state->viewMode = 0;
        } else {
            state->toggleLabel = " Intercept: OFF ";
            state->viewMode = 1;
        }
    });

    auto forwardButton = Button(" Forward ", [=] {
        auto cur = InterceptManager::instance().peekCurrent();
        if (cur && cur->request) {
            std::string editedText = interceptEditor->GetText();
            *cur->request = ParseRawHttpRequest(editedText);
        }
        InterceptManager::instance().forwardCurrent();
    });

    auto dropButton = Button(" Drop ", [=] {
        InterceptManager::instance().dropCurrent();
    });

    auto toRepeaterButton = Button(" ↻ To Repeater ", [=] {
        bool interceptOn = InterceptManager::instance().isInterceptEnabled();
        if (interceptOn) {
            auto cur = InterceptManager::instance().peekCurrent();
            if (cur && cur->request) {
                RepeaterManager::instance().setRequest(
                    cur->host,
                    std::to_string(cur->port),
                    cur->isHttps,
                    interceptEditor->GetText());
            }
        } else {
            if (entries && !entries->empty()) {
                const auto& latest = entries->back();
                if (latest.request) {
                    std::string raw = FormatHttpRequestRaw(*latest.request, latest.is_https, latest.host);
                    RepeaterManager::instance().setRequest(
                        latest.host,
                        std::to_string(latest.port),
                        latest.is_https,
                        raw);
                }
            }
        }
    });

    auto copyReqButton = Button(" Copy Req ", [=] {
        bool interceptOn = InterceptManager::instance().isInterceptEnabled();
        if (interceptOn) {
            interceptEditor->CopyToClipboard();
        } else {
            liveReqViewer->CopyToClipboard();
        }
    });

    auto copyResButton = Button(" Copy Res ", [=] {
        liveResViewer->CopyToClipboard();
    });

    auto mainLayout = Container::Vertical({
        toggleButton,
        forwardButton,
        dropButton,
        toRepeaterButton,
        copyReqButton,
        copyResButton,
        interceptEditor,
        liveReqViewer,
        liveResViewer,
    });

    return Renderer(mainLayout, [=] {
        bool interceptOn = InterceptManager::instance().isInterceptEnabled();
        state->toggleLabel = interceptOn ? " Intercept: ON  " : " Intercept: OFF ";
        state->viewMode = interceptOn ? 0 : 1;

        auto btnToggle = toggleButton->Render();
        if (interceptOn) {
            btnToggle = btnToggle | color(Color::Red) | bold;
        } else {
            btnToggle = btnToggle | color(Color::Green) | bold;
        }

        bool hasPending = InterceptManager::instance().hasPending();
        auto btnForward = forwardButton->Render();
        auto btnDrop = dropButton->Render();

        if (interceptOn && hasPending) {
            btnForward = btnForward | color(Color::GreenLight) | bold;
            btnDrop = btnDrop | color(Color::RedLight) | bold;
        } else if (!interceptOn) {
            btnForward = btnForward | dim;
            btnDrop = btnDrop | dim;
        }

        auto btnToRepeater = toRepeaterButton->Render() | color(Color::Cyan);
        auto btnCopyReq = copyReqButton->Render();
        auto btnCopyRes = copyResButton->Render();

        Element mainPanelContent;

        if (interceptOn) {
            auto currentItem = InterceptManager::instance().peekCurrent();
            if (currentItem && currentItem->request) {
                if (currentItem->id != state->lastInterceptId) {
                    state->lastInterceptId = currentItem->id;
                    std::string raw = FormatHttpRequestRaw(*currentItem->request, currentItem->isHttps, currentItem->host);
                    interceptEditor->SetText(raw);
                }

                std::size_t pendingCount = InterceptManager::instance().pendingCount();
                std::string proto = currentItem->isHttps ? "https://" : "http://";
                std::string fullUrl = proto + currentItem->host + currentItem->request->url;

                mainPanelContent = vbox(Elements{
                    hbox(Elements{
                        text("  ⚠ INTERCEPTED REQUEST PAUSED (Editable): ") | bold | color(Color::RedLight),
                        text(currentItem->request->method + " " + fullUrl) | bold | color(Color::White) | xflex_shrink,
                        filler(),
                        text("[ " + std::to_string(pendingCount) + (pendingCount == 1 ? " request queued ]  " : " requests queued ]  ")) | bold | color(Color::Yellow),
                    }),
                    separatorLight(),
                    interceptEditor->Render() | flex,
                });
            } else {
                state->lastInterceptId = -1;
                mainPanelContent = vbox(Elements{
                    filler(),
                    text("  ● Intercept is ON  ") | bold | color(Color::RedLight) | center,
                    text("") | center,
                    text("Waiting for incoming HTTP/HTTPS browser requests...") | bold | color(Color::White) | center,
                    text("") | center,
                    text("Any request sent through the proxy will pause here for editing & inspection.") | dim | center,
                    text("Edit request directly, click [ Forward ] to send, or [ Drop ] to reject.") | dim | center,
                    text("Click [ ↻ To Repeater ] to copy the request to Repeater.") | dim | center,
                    filler(),
                });
            }
        } else {
            if (entries && !entries->empty()) {
                const auto& latest = entries->back();

                if (latest.id != state->lastDisplayedTxId) {
                    state->lastDisplayedTxId = latest.id;
                    if (latest.request) {
                        auto reqLines = FormatHttpRequestLines(*latest.request, latest.is_https, latest.host);
                        std::string reqRaw = FormatHttpRequestRaw(*latest.request, latest.is_https, latest.host);
                        liveReqViewer->SetContent(std::move(reqLines), std::move(reqRaw));
                    }
                    if (latest.response) {
                        auto resLines = FormatHttpResponseLines(*latest.response);
                        std::string resRaw = FormatHttpResponseRaw(*latest.response);
                        liveResViewer->SetContent(std::move(resLines), std::move(resRaw));
                    }
                }

                std::string proto = latest.is_https ? "https://" : "http://";
                std::string urlStr = (latest.request) ? (proto + latest.host + latest.request->url) : "";
                std::string statusStr = (latest.response) ? std::to_string(latest.response->statusCode) : "-";

                mainPanelContent = vbox(Elements{
                    hbox(Elements{
                        text("  Latest #" + std::to_string(latest.id) + "  ") | bold | color(Color::Cyan),
                        text((latest.request ? latest.request->method : "") + " " + urlStr) | color(Color::White) | xflex_shrink,
                        filler(),
                        text("Status: " + statusStr + "  ") | bold | color(Color::Green),
                    }),
                    separatorLight(),
                    hbox(Elements{
                        liveReqViewer->Render() | flex,
                        separatorLight(),
                        liveResViewer->Render() | flex,
                    }) | flex,
                });
            } else {
                mainPanelContent = vbox(Elements{
                    filler(),
                    text("No HTTP/HTTPS traffic captured yet.") | dim | center,
                    text("Configure your browser proxy to " + cfg.listenHost + ":" + std::to_string(cfg.listenPort)) | dim | center,
                    text("CA Certificate download: http://burp/cert or http://" + cfg.listenHost + ":" + std::to_string(cfg.listenPort) + "/cert") | dim | center,
                    filler(),
                });
            }
        }

        return vbox(Elements{
            hbox(Elements{
                btnToggle,
                separatorLight(),
                btnForward,
                btnDrop,
                separatorLight(),
                btnToRepeater,
                separatorLight(),
                btnCopyReq,
                btnCopyRes,
                filler(),
                text("Proxy: " + cfg.listenHost + ":" + std::to_string(cfg.listenPort) + " ") | dim,
            }),
            separatorLight(),
            mainPanelContent | flex,
            Widgets::StatusBar(
                interceptOn ? "PROXY INTERCEPTING" : "PROXY STREAMING",
                interceptOn ? (hasPending ? "Request awaiting forward/drop decision" : "Idle - waiting for browser traffic")
                            : (entries->empty() ? "Listening for traffic" : ("Captured " + std::to_string(entries->size()) + " requests"))
            ),
        });
    });
}

} // namespace BurpTUI

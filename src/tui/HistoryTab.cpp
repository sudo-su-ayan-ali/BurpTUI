#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include "tui/HttpFormatter.hpp"
#include "tui/ScrollableViewer.hpp"
#include "tui/RepeaterManager.hpp"
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

} // anonymous namespace

Component MakeHistoryTab(
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue,
    ftxui::ScreenInteractive* screen,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries)
{
    (void)screen;
    auto entries = sharedEntries ? sharedEntries : std::make_shared<std::vector<HttpTransaction>>();
    auto entryLabels = std::make_shared<std::vector<std::string>>();
    auto selectedIndex = std::make_shared<int>(0);
    auto queue = txQueue;

    struct State {
        int lastLoadedIdx = -1;
    };
    auto state = std::make_shared<State>();

    auto reqViewer = MakeScrollableViewer("Request");
    auto resViewer = MakeScrollableViewer("Response");

    auto sendToRepeaterAction = [=]() {
        if (entries->empty()) return;
        int idx = *selectedIndex;
        if (idx < 0 || idx >= static_cast<int>(entries->size())) return;
        const auto& tx = (*entries)[idx];
        if (tx.request) {
            std::string raw = FormatHttpRequestRaw(*tx.request, tx.is_https, tx.host);
            RepeaterManager::instance().setRequest(tx.host, std::to_string(tx.port), tx.is_https, raw);
        }
    };

    auto toRepeaterBtn = Button(" ↻ Send to Repeater [r] ", sendToRepeaterAction);
    auto copyReqBtn    = Button(" Copy Req [y] ", [=] { reqViewer->CopyToClipboard(); });
    auto copyResBtn    = Button(" Copy Res [c] ", [=] { resViewer->CopyToClipboard(); });

    // Pre-populate labels if entries already exist
    for (const auto& tx : *entries) {
        entryLabels->push_back(BuildMenuLabel(tx));
    }

    auto menuComp = Menu(entryLabels.get(), selectedIndex.get());

    auto rightControls = Container::Horizontal({
        toRepeaterBtn,
        copyReqBtn,
        copyResBtn,
    });

    auto detailLayout = Container::Vertical({
        rightControls,
        reqViewer,
        resViewer,
    });

    auto layout = Container::Horizontal({menuComp, detailLayout});

    auto updateActiveDetails = [=]() {
        if (entries->empty()) {
            state->lastLoadedIdx = -1;
            reqViewer->SetContent({});
            resViewer->SetContent({});
            return;
        }

        int idx = *selectedIndex;
        if (idx < 0) idx = 0;
        if (idx >= static_cast<int>(entries->size())) idx = static_cast<int>(entries->size()) - 1;

        if (idx != state->lastLoadedIdx) {
            state->lastLoadedIdx = idx;
            const auto& active = (*entries)[idx];
            if (active.request) {
                reqViewer->SetContent(
                    FormatHttpRequestLines(*active.request, active.is_https, active.host),
                    FormatHttpRequestRaw(*active.request, active.is_https, active.host)
                );
            } else {
                reqViewer->SetContent({text("(No request data)") | dim});
            }

            if (active.response) {
                resViewer->SetContent(
                    FormatHttpResponseLines(*active.response),
                    FormatHttpResponseRaw(*active.response)
                );
            } else {
                resViewer->SetContent({text("(Response pending...)") | dim});
            }
        }
    };

    layout = CatchEvent(layout, [=](Event event) {
        if (event == Event::Custom) {
            auto newItems = queue->tryPopAll();
            for (auto& tx : newItems) {
                entryLabels->push_back(BuildMenuLabel(tx));
                entries->push_back(std::move(tx));
            }
            updateActiveDetails();
            return false;
        }

        // Only handle j/k for menu navigation if menu is focused
        if (menuComp->Focused()) {
            if (event == Event::Character('j')) {
                if (*selectedIndex + 1 < static_cast<int>(entries->size())) {
                    (*selectedIndex)++;
                    updateActiveDetails();
                }
                return true;
            }
            if (event == Event::Character('k')) {
                if (*selectedIndex > 0) {
                    (*selectedIndex)--;
                    updateActiveDetails();
                }
                return true;
            }
            if (event == Event::ArrowDown || event == Event::ArrowUp) {
                // Let menu process, then update details
                bool res = menuComp->OnEvent(event);
                updateActiveDetails();
                return res;
            }
        }

        if (event == Event::Character('r') || event == Event::Character('R')) {
            sendToRepeaterAction();
            return true;
        }
        if (event == Event::Character('y') && !menuComp->Focused()) {
            reqViewer->CopyToClipboard();
            return true;
        }
        if (event == Event::Character('c') && !menuComp->Focused()) {
            resViewer->CopyToClipboard();
            return true;
        }

        return false;
    });

    return Renderer(layout, [=] {
        updateActiveDetails();

        std::string selectedStatus = "0 / 0";
        if (!entries->empty()) {
            selectedStatus = std::to_string(*selectedIndex + 1) + " / " + std::to_string(entries->size());
        }

        auto leftPanel = vbox(Elements{
            text(" Captured Transactions ") | bold | color(Color::CyanLight),
            separatorLight(),
            menuComp->Render() | vscroll_indicator | frame | flex,
        }) | flex;

        auto detailPanel = vbox(Elements{
            hbox(Elements{
                toRepeaterBtn->Render(),
                separatorLight(),
                copyReqBtn->Render(),
                copyResBtn->Render(),
                filler(),
                text("Selected: " + selectedStatus + " ") | dim,
            }),
            separatorLight(),
            reqViewer->Render() | flex,
            separatorLight(),
            resViewer->Render() | flex,
        }) | flex;

        return vbox(Elements{
            hbox(Elements{
                leftPanel | size(WIDTH, EQUAL, 58),
                separatorLight(),
                detailPanel | flex,
            }) | flex,
            Widgets::StatusBar("History Active", "Navigate list: Tab/Arrows | Send to Repeater: [r] | Copy: [y]/[c]"),
        });
    });
}

} // namespace BurpTUI

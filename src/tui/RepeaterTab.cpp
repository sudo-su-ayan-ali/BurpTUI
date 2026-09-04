#include "tui/RepeaterTab.hpp"
#include "tui/Widgets.hpp"
#include "tui/HttpFormatter.hpp"
#include "tui/ScrollableViewer.hpp"
#include "tui/TextEditor.hpp"
#include "tui/RepeaterManager.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

namespace BurpTUI {

using namespace ftxui;

Component MakeRepeaterTab() {
    auto reqEditor = MakeTextEditor("Request (Editable)");
    auto resViewer = MakeScrollableViewer("Response (Scrollable)");

    struct State {
        std::string host;
        std::string port;
        bool isHttps{true};
        std::string lastLoadedReq;
        std::string lastLoadedRes;
    };
    auto state = std::make_shared<State>();
    state->host = RepeaterManager::instance().getHost();
    state->port = RepeaterManager::instance().getPort();
    state->isHttps = RepeaterManager::instance().isHttps();
    state->lastLoadedReq = RepeaterManager::instance().getRawRequest();
    state->lastLoadedRes = RepeaterManager::instance().getRawResponse();

    reqEditor->SetText(state->lastLoadedReq);
    resViewer->SetRawText(state->lastLoadedRes);

    auto inputHost = Input(&state->host, "Host (e.g. httpbin.org)");
    auto inputPort = Input(&state->port, "Port");
    auto checkHttps = Checkbox(" HTTPS", &state->isHttps);

    auto sendButton = Button(" Send ", [=] {
        RepeaterManager::instance().setTarget(state->host, state->port, state->isHttps);
        RepeaterManager::instance().setRawRequest(reqEditor->GetText());
        RepeaterManager::instance().executeRequest();
    });

    auto copyReqBtn = Button(" Copy Req ", [=] {
        reqEditor->CopyToClipboard();
    });

    auto copyResBtn = Button(" Copy Res ", [=] {
        resViewer->CopyToClipboard();
    });

    auto container = Container::Vertical({
        inputHost,
        inputPort,
        checkHttps,
        sendButton,
        copyReqBtn,
        copyResBtn,
        reqEditor,
        resViewer,
    });

    auto syncWithManager = [=]() {
        std::string curHost = RepeaterManager::instance().getHost();
        std::string curPort = RepeaterManager::instance().getPort();
        bool curHttps = RepeaterManager::instance().isHttps();
        std::string curReq = RepeaterManager::instance().getRawRequest();
        std::string curRes = RepeaterManager::instance().getRawResponse();

        if (curReq != state->lastLoadedReq) {
            state->lastLoadedReq = curReq;
            reqEditor->SetText(curReq);
            state->host = curHost;
            state->port = curPort;
            state->isHttps = curHttps;
        }
        if (curRes != state->lastLoadedRes) {
            state->lastLoadedRes = curRes;
            resViewer->SetRawText(curRes);
        }
    };

    auto renderer = Renderer(container, [=] {
        syncWithManager();

        bool sending = RepeaterManager::instance().isSending();
        std::string status = RepeaterManager::instance().getStatusText();

        auto btnSend = sendButton->Render();
        if (sending) {
            btnSend = btnSend | color(Color::Yellow) | bold;
        } else {
            btnSend = btnSend | color(Color::GreenLight) | bold;
        }

        auto targetBar = hbox(Elements{
            text(" Target: ") | bold,
            text(state->isHttps ? "https://" : "http://") | color(Color::Cyan),
            inputHost->Render() | size(WIDTH, EQUAL, 24) | borderLight,
            text(" : ") | bold,
            inputPort->Render() | size(WIDTH, EQUAL, 7) | borderLight,
            separatorLight(),
            checkHttps->Render(),
            separatorLight(),
            btnSend,
            separatorLight(),
            copyReqBtn->Render(),
            copyResBtn->Render(),
            filler(),
            text(sending ? " [SENDING...] " : (" [" + status + "] ")) | bold | (sending ? color(Color::Yellow) : color(Color::Green)),
        });

        return vbox(Elements{
            targetBar,
            separatorLight(),
            hbox(Elements{
                vbox(Elements{
                    text(" Request Editor (Editable | Tab/Arrows | Type/Edit) ") | bold | color(Color::Cyan),
                    reqEditor->Render() | flex,
                }) | flex,
                separatorLight(),
                vbox(Elements{
                    text(" Response Viewer (Scrollable | Up/Down/Wheel) ") | bold | color(Color::Green),
                    resViewer->Render() | flex,
                }) | flex,
            }) | flex,
            Widgets::StatusBar("Repeater Active", status),
        });
    });

    return CatchEvent(renderer, [=](Event event) {
        if (event == Event::Custom) {
            syncWithManager();
            return false;
        }
        return false;
    });
}

} // namespace BurpTUI

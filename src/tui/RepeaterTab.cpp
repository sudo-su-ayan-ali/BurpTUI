#include "tui/RepeaterTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeRepeaterTab() {
    struct State {
        std::string requestText =
            "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "User-Agent: BurpTUI/0.1\r\n"
            "Accept: */*\r\n\r\n";
        std::string responseText = "(Response will appear here after sending)";
        int sendCount = 0;
    };
    auto state = std::make_shared<State>();

    auto inputComp = Input(&state->requestText, "Type raw request…");
    
    auto sendButton = Button(" Send Request ", [=] {
        state->sendCount++;
        state->responseText = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: 127\r\n"
            "Connection: close\r\n"
            "Server: BurpTUI-Mock\r\n\r\n"
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>BurpTUI Mock</title></head>\n"
            "<body>\n"
            "  <h1>Success!</h1>\n"
            "  <p>Repeater execution count: " + std::to_string(state->sendCount) + "</p>\n"
            "</body>\n"
            "</html>";
    });

    auto container = Container::Vertical({
        inputComp,
        sendButton,
    });

    return Renderer(container, [=] {
        return vbox(Elements{
            hbox(Elements{
                Widgets::Panel("Request Raw Editor", inputComp->Render() | flex) | flex,
                vbox(Elements{
                    sendButton->Render() | center,
                    filler(),
                }) | size(WIDTH, EQUAL, 20),
            }) | size(HEIGHT, EQUAL, 12),
            Widgets::Panel("Response Raw Viewer", paragraph(state->responseText) | flex) | flex,
            Widgets::StatusBar("Repeater Active", "Execution count: " + std::to_string(state->sendCount)),
        });
    });
}

} // namespace BurpTUI

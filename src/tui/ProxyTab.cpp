#include "tui/ProxyTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeProxyTab() {
    return Renderer([] {
        return vbox({
            Widgets::Panel("Intercepted Requests",
                vbox({
                    hbox({ text("  #") | size(WIDTH, EQUAL, 5),
                           text("Method")  | size(WIDTH, EQUAL, 10),
                           text("Host")    | size(WIDTH, EQUAL, 30),
                           text("URL")     | flex }) | bold,
                    separator(),
                    text("  (No intercepted traffic yet)") | dim | center | flex,
                })),
            Widgets::StatusBar("Intercept: OFF", "0 requests"),
        });
    });
}

} // namespace BurpTUI

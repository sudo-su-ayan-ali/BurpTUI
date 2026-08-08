#include "tui/HistoryTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeHistoryTab() {
    return Renderer([] {
        return vbox({
            Widgets::Panel("Request History",
                vbox({
                    hbox({ text("  #") | size(WIDTH, EQUAL, 5),
                           text("Method")  | size(WIDTH, EQUAL, 10),
                           text("Status")  | size(WIDTH, EQUAL, 8),
                           text("Host")    | size(WIDTH, EQUAL, 30),
                           text("URL")     | flex }) | bold,
                    separator(),
                    text("  (No history yet)") | dim | center | flex,
                })),
            Widgets::StatusBar("History", "0 entries"),
        });
    });
}

} // namespace BurpTUI

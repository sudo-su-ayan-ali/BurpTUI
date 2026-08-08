#include "tui/RepeaterTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeRepeaterTab() {
    auto state = std::make_shared<std::string>();

    auto input = Input(state.get(), "Paste or type a raw HTTP request…");

    return Renderer(input, [input] {
        return vbox({
            Widgets::Panel("Request Editor", input->Render() | flex),
            separator(),
            Widgets::Panel("Response", text("  (Send a request to see the response)") | dim | center | flex),
            Widgets::StatusBar("Repeater", "Press F5 to send"),
        });
    });
}

} // namespace BurpTUI

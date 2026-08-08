#include "tui/DecoderTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeDecoderTab() {
    auto inputText  = std::make_shared<std::string>();
    auto outputText = std::make_shared<std::string>();
    int  selected   = 0;
    std::vector<std::string> modes = {
        "Base64 Encode", "Base64 Decode",
        "URL Encode",    "URL Decode",
        "Hex Encode",    "Hex Decode",
    };

    auto inputComp = Input(inputText.get(),  "Input…");
    auto modeMenu  = Radiobox(&modes, &selected);

    auto container = Container::Horizontal({modeMenu, inputComp});

    return Renderer(container, [=] {
        return vbox({
            hbox({
                Widgets::Panel("Mode", modeMenu->Render()) | size(WIDTH, EQUAL, 20),
                Widgets::Panel("Input", inputComp->Render() | flex) | flex,
            }) | flex,
            Widgets::Panel("Output",
                text(*outputText) | flex),
            Widgets::StatusBar("Decoder", ""),
        });
    });
}

} // namespace BurpTUI

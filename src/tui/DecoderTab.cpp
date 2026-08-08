#include "tui/DecoderTab.hpp"
#include "tui/Widgets.hpp"
#include "util/Encoding.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeDecoderTab() {
    struct State {
        std::string inputText;
        std::string outputText;
        int selectedMode = 0;
    };
    auto state = std::make_shared<State>();

    std::vector<std::string> modes = {
        "Base64 Encode", "Base64 Decode",
        "URL Encode",    "URL Decode",
        "Hex Encode",    "Hex Decode",
    };

    auto inputComp = Input(&state->inputText, "Type text to transform…");
    auto modeMenu  = Radiobox(&modes, &state->selectedMode);

    auto container = Container::Horizontal({
        modeMenu,
        inputComp,
    });

    return Renderer(container, [=] {
        // Run transformation logic on every render
        try {
            if (state->selectedMode == 0) {
                state->outputText = Encoding::base64Encode(state->inputText);
            } else if (state->selectedMode == 1) {
                state->outputText = Encoding::base64Decode(state->inputText);
            } else if (state->selectedMode == 2) {
                state->outputText = Encoding::urlEncode(state->inputText);
            } else if (state->selectedMode == 3) {
                state->outputText = Encoding::urlDecode(state->inputText);
            } else if (state->selectedMode == 4) {
                state->outputText = Encoding::hexEncode(state->inputText);
            } else if (state->selectedMode == 5) {
                state->outputText = Encoding::hexDecode(state->inputText);
            }
        } catch (const std::exception& e) {
            state->outputText = std::string("Error: ") + e.what();
        }

        return vbox(Elements{
            hbox(Elements{
                Widgets::Panel("Transform Mode", modeMenu->Render()) | size(WIDTH, EQUAL, 24),
                Widgets::Panel("Input Buffer", inputComp->Render() | flex) | flex,
            }) | size(HEIGHT, EQUAL, 10),
            Widgets::Panel("Output Result", paragraph(state->outputText) | flex) | flex,
            Widgets::StatusBar("Decoder Active", "Transform count: " + std::to_string(state->inputText.size())),
        });
    });
}

} // namespace BurpTUI

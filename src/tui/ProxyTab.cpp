#include "tui/ProxyTab.hpp"
#include "tui/Widgets.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace BurpTUI {

using namespace ftxui;

ftxui::Component MakeProxyTab() {
    struct State {
        bool interceptOn = false;
        std::string toggleLabel = " Intercept: OFF ";
        int interceptedCount = 0;
        int forwardedCount = 0;
        int droppedCount = 0;
    };
    auto state = std::make_shared<State>();

    // Use dynamic string reference for the button label
    auto toggleButton = Button(&state->toggleLabel, [=] {
        state->interceptOn = !state->interceptOn;
        if (state->interceptOn) {
            state->toggleLabel = " Intercept: ON  ";
            state->interceptedCount = 1; // Simulate one intercepted request
        } else {
            state->toggleLabel = " Intercept: OFF ";
            state->interceptedCount = 0;
        }
    });

    auto forwardButton = Button(" Forward ", [=] {
        if (state->interceptedCount > 0) {
            state->interceptedCount--;
            state->forwardedCount++;
        }
    });

    auto dropButton = Button(" Drop ", [=] {
        if (state->interceptedCount > 0) {
            state->interceptedCount--;
            state->droppedCount++;
        }
    });

    auto container = Container::Horizontal({
        toggleButton,
        forwardButton,
        dropButton,
    });

    return Renderer(container, [=] {
        auto btnToggle = toggleButton->Render();
        if (state->interceptOn) {
            btnToggle = btnToggle | color(Color::Red);
        } else {
            btnToggle = btnToggle | color(Color::Green);
        }

        auto btnForward = forwardButton->Render();
        auto btnDrop = dropButton->Render();

        Element interceptedRequestDetails = filler();
        if (state->interceptOn && state->interceptedCount > 0) {
            interceptedRequestDetails = vbox(Elements{
                text("GET /v1/api/secret HTTP/1.1") | bold | color(Color::Yellow),
                text("Host: staging.internal.lan"),
                text("Authorization: Bearer secret_token_abc123"),
                text("Accept-Language: en-US,en;q=0.9"),
                filler(),
            });
        } else {
            interceptedRequestDetails = text("  (No requests currently intercepted. Toggle intercept ON to capture.)") 
                | dim | center | flex;
        }

        return vbox(Elements{
            hbox(Elements{
                btnToggle,
                separator(),
                btnForward,
                separator(),
                btnDrop,
            }) | size(HEIGHT, EQUAL, 3),
            separator(),
            Widgets::Panel("Live Intercepted Buffer", interceptedRequestDetails) | flex,
            Widgets::StatusBar(
                state->interceptOn ? "INTERCEPTING LIVE TRAFFIC" : "PAUSED (PASS-THROUGH)", 
                "Forwarded: " + std::to_string(state->forwardedCount) + "  |  Dropped: " + std::to_string(state->droppedCount)
            ),
        });
    });
}

} // namespace BurpTUI

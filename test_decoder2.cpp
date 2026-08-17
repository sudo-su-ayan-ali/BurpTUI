#include "tui/DecoderTab.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
int main() {
    std::cout << "Starting\n";
    auto tab = BurpTUI::MakeDecoderTab();
    std::cout << "Tab created\n";
    auto doc = tab->Render();
    std::cout << "Rendered\n";
    return 0;
}

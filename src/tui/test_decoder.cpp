#include "tui/DecoderTab.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
int main() {
    auto tab = BurpTUI::MakeDecoderTab();
    std::cout << "MakeDecoderTab called\n";
    auto doc = tab->Render();
    std::cout << "Render called\n";
    return 0;
}

#include "tui/TuiApp.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
int main() {
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    BurpTUI::Config cfg;
    auto txQueue = std::make_shared<BurpTUI::TsQueue<BurpTUI::HttpTransaction>>();
    BurpTUI::TuiApp app(cfg, txQueue);
    
    // We can't easily send mouse events inside TuiApp without hacking it.
    // Let's just instantiate and render it.
    return 0;
}

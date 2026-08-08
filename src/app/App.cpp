#include "App.hpp"
#include "tui/TuiApp.hpp"

namespace BurpTUI {

App::App(Config cfg) : cfg_(std::move(cfg)) {}
App::~App() = default;

int App::run() {
    TuiApp tui(cfg_);
    tui.run();
    return 0;
}

} // namespace BurpTUI

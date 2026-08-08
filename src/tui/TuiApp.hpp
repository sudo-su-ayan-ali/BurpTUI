#pragma once
#include "app/Config.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace BurpTUI {

/// Root TUI controller — owns the screen and tab components.
class TuiApp {
public:
    explicit TuiApp(const Config& cfg);
    ~TuiApp();

    void run();   ///< Blocks until the user quits.

private:
    const Config&                    cfg_;
    ftxui::ScreenInteractive         screen_;
    ftxui::Component                 root_;
    
    int                              activeTab_ = 0;
    std::vector<std::string>         tabNames_;

    void buildLayout();
};

} // namespace BurpTUI

#pragma once
#include "app/Config.hpp"
#include "util/TsQueue.hpp"
#include "http/HttpTransaction.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>

namespace BurpTUI {

class TuiApp {
public:
    TuiApp(const Config& cfg,
           std::shared_ptr<TsQueue<HttpTransaction>> txQueue);
    ~TuiApp();
    void run();

private:
    const Config& cfg_;
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue_;
    ftxui::ScreenInteractive screen_;
    ftxui::Component root_;
    int activeTab_ = 0;
    std::vector<std::string> tabNames_;
    void buildLayout();
};

} // namespace BurpTUI

#include "tui/TuiApp.hpp"
#include "tui/ProxyTab.hpp"
#include "tui/HistoryTab.hpp"
#include "tui/RepeaterTab.hpp"
#include "tui/DecoderTab.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace BurpTUI {

using namespace ftxui;

TuiApp::TuiApp(const Config& cfg,
               std::shared_ptr<TsQueue<HttpTransaction>> txQueue)
    : cfg_(cfg)
    , txQueue_(std::move(txQueue))
    , screen_(ScreenInteractive::Fullscreen())
{
    buildLayout();
}

TuiApp::~TuiApp() = default;

std::function<void()> TuiApp::getUpdateTrigger() {
    return [this]() {
        screen_.PostEvent(ftxui::Event::Custom);
    };
}

void TuiApp::buildLayout() {
    tabNames_ = {" Proxy ", " History ", " Repeater ", " Decoder "};
    auto tabToggle = Toggle(&tabNames_, &activeTab_);

    auto historyTab = MakeHistoryTab(txQueue_, &screen_);

    auto tabContents = Container::Tab(
        {
            MakeProxyTab(),
            historyTab,
            MakeRepeaterTab(),
            MakeDecoderTab(),
        },
        &activeTab_);

    auto layout = Container::Vertical({tabToggle, tabContents});

    root_ = Renderer(layout, [&, tabToggle, tabContents] {
        return vbox(Elements{
            text(" BurpTUI ") | bold | center,
            separator(),
            tabToggle->Render(),
            separator(),
            tabContents->Render() | flex,
            separator(),
            hbox(Elements{
                text("  q: Quit  |  Tab: Switch Tabs  |  Proxy: " + cfg_.listenHost + ":" + std::to_string(cfg_.listenPort)) | dim,
                filler(),
                text("BurpTUI v0.1  ") | dim,
            }),
        });
    });

    root_ = CatchEvent(root_, [=, this](Event event) {
        if (event == Event::Custom) {
            // Always dispatch Custom event to HistoryTab so queue is drained
            // even when user is on another tab (Proxy, Repeater, etc.)
            historyTab->OnEvent(Event::Custom);
            return false;
        }
        if (event == Event::Character('q') || event == Event::Special("\x03")) {
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Tab) {
            activeTab_ = (activeTab_ + 1) % tabNames_.size();
            historyTab->OnEvent(Event::Custom);
            return true;
        }
        if (event == Event::TabReverse) {
            activeTab_ = (activeTab_ + tabNames_.size() - 1) % tabNames_.size();
            historyTab->OnEvent(Event::Custom);
            return true;
        }
        return false;
    });
}

void TuiApp::run() {
    screen_.Loop(root_);
}

} // namespace BurpTUI

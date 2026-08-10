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

void TuiApp::buildLayout() {
    tabNames_ = {" Proxy ", " History ", " Repeater ", " Decoder "};
    auto tabToggle = Toggle(&tabNames_, &activeTab_);

    auto tabContents = Container::Tab(
        {
            MakeProxyTab(),
            MakeHistoryTab(txQueue_, &screen_),
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
                text("  q: Quit") | dim,
                filler(),
                text("BurpTUI v0.1  ") | dim,
            }),
        });
    });

    root_ = CatchEvent(root_, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Special("\x03")) {
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Tab) {
            activeTab_ = (activeTab_ + 1) % tabNames_.size();
            return true;
        }
        if (event == Event::TabReverse) {
            activeTab_ = (activeTab_ + tabNames_.size() - 1) % tabNames_.size();
            return true;
        }
        return false;
    });
}

void TuiApp::run() {
    screen_.Loop(root_);
}

} // namespace BurpTUI

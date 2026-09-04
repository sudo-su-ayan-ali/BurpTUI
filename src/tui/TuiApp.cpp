#include "tui/TuiApp.hpp"
#include "tui/ProxyTab.hpp"
#include "tui/HistoryTab.hpp"
#include "tui/RepeaterTab.hpp"
#include "tui/DecoderTab.hpp"
#include "tui/RepeaterManager.hpp"
#include "proxy/InterceptManager.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>

namespace BurpTUI {

using namespace ftxui;

TuiApp::TuiApp(const Config& cfg,
               std::shared_ptr<TsQueue<HttpTransaction>> txQueue)
    : cfg_(cfg)
    , txQueue_(std::move(txQueue))
    , screen_(ScreenInteractive::Fullscreen())
{
    mouseTracking_ = cfg_.enableMouse;
    screen_.TrackMouse(cfg_.enableMouse);
    buildLayout();
}

TuiApp::~TuiApp() = default;

std::function<void()> TuiApp::getUpdateTrigger() {
    return [this]() {
        bool expected = false;
        if (updatePending_.compare_exchange_strong(expected, true)) {
            screen_.PostEvent(ftxui::Event::Custom);
        }
    };
}

void TuiApp::buildLayout() {
    tabNames_ = {" Proxy [F1] ", " History [F2] ", " Repeater [F3] ", " Decoder [F4] "};
    auto tabToggle = Toggle(&tabNames_, &activeTab_);

    auto sharedEntries = std::make_shared<std::vector<HttpTransaction>>();
    auto proxyTab = MakeProxyTab(cfg_, sharedEntries);
    auto historyTab = MakeHistoryTab(txQueue_, &screen_, sharedEntries);
    auto repeaterTab = MakeRepeaterTab();
    auto decoderTab = MakeDecoderTab();

    // Wire RepeaterManager callbacks
    RepeaterManager::instance().setNotifyCallback(getUpdateTrigger());
    RepeaterManager::instance().setSwitchTabCallback([this, historyTab, repeaterTab](int tabIdx) {
        activeTab_ = tabIdx;
        screen_.PostEvent(Event::Custom);
    });

    auto tabContents = Container::Tab(
        {
            proxyTab,
            historyTab,
            repeaterTab,
            decoderTab,
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
                text("  q: Quit  |  1-4 / F1-F4: Tabs  |  Click/Tap Buttons Active  |  Drag text to select & copy  |  Right-click: Copy word") | dim,
                filler(),
                text("Proxy: " + cfg_.listenHost + ":" + std::to_string(cfg_.listenPort) + "  ") | dim,
            }),
        });
    });

    root_ = CatchEvent(root_, [=, this](Event event) {
        if (event == Event::Custom) {
            updatePending_.store(false);
            historyTab->OnEvent(Event::Custom);
            proxyTab->OnEvent(Event::Custom);
            repeaterTab->OnEvent(Event::Custom);
            return false;
        }

        // Unconditional global shortcuts
        if (event == Event::Special("\x11") || event == Event::Special("\x03")) {
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event == Event::F1) { activeTab_ = 0; return true; }
        if (event == Event::F2) { activeTab_ = 1; historyTab->OnEvent(Event::Custom); return true; }
        if (event == Event::F3) { activeTab_ = 2; repeaterTab->OnEvent(Event::Custom); return true; }
        if (event == Event::F4) { activeTab_ = 3; return true; }

        // Let components process the event first (typing in editor, mouse clicks on buttons, etc.)
        if (layout->OnEvent(event)) {
            return true;
        }

        // Fallback hotkeys if not consumed by an active editor/input:
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Character('1')) { activeTab_ = 0; return true; }
        if (event == Event::Character('2')) { activeTab_ = 1; historyTab->OnEvent(Event::Custom); return true; }
        if (event == Event::Character('3')) { activeTab_ = 2; repeaterTab->OnEvent(Event::Custom); return true; }
        if (event == Event::Character('4')) { activeTab_ = 3; return true; }

        return false;
    });
}

void TuiApp::run() {
    screen_.Loop(root_);
}

} // namespace BurpTUI

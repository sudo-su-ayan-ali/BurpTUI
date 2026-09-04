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
        std::string mouseStatus = mouseTracking_
            ? "Mouse: UI Mode (Hold Shift to select text)"
            : "Text Select & Right-Click Active";

        return vbox(Elements{
            text(" BurpTUI ") | bold | center,
            separator(),
            tabToggle->Render(),
            separator(),
            tabContents->Render() | flex,
            separator(),
            hbox(Elements{
                text("  Ctrl+Q: Quit  |  F1-F4: Tabs  |  Tab: Navigate  |  " + mouseStatus) | dim,
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

        // Quit with Ctrl+Q or Ctrl+C
        if (event == Event::Special("\x11") || event == Event::Special("\x03")) {
            screen_.ExitLoopClosure()();
            return true;
        }

        // F1 - F4 to switch tabs
        if (event == Event::F1) {
            activeTab_ = 0;
            return true;
        }
        if (event == Event::F2) {
            activeTab_ = 1;
            historyTab->OnEvent(Event::Custom);
            return true;
        }
        if (event == Event::F3) {
            activeTab_ = 2;
            repeaterTab->OnEvent(Event::Custom);
            return true;
        }
        if (event == Event::F4) {
            activeTab_ = 3;
            return true;
        }

        // F9 to toggle Mouse Tracking vs Terminal Text Selection
        if (event == Event::F9) {
            mouseTracking_ = !mouseTracking_;
            if (mouseTracking_) {
                std::cout << "\033[?1000h\033[?1002h\033[?1006h" << std::flush;
            } else {
                std::cout << "\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?1015l\033[?9l" << std::flush;
            }
            return true;
        }

        return false;
    });
}

void TuiApp::run() {
    screen_.Loop(root_);
}

} // namespace BurpTUI

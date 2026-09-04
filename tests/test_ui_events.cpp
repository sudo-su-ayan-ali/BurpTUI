#include <gtest/gtest.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include "tui/ProxyTab.hpp"
#include "tui/HistoryTab.hpp"
#include "tui/RepeaterTab.hpp"
#include "app/Config.hpp"
#include "util/TsQueue.hpp"

using namespace ftxui;
using namespace BurpTUI;

TEST(UIEventsTest, ButtonClicks) {
    bool clicked = false;
    auto btn = Button("Test", [&] { clicked = true; });
    btn->TakeFocus();
    btn->OnEvent(Event::Return);
    EXPECT_TRUE(clicked);
}

TEST(UIEventsTest, ProxyTabInstantiation) {
    Config cfg;
    auto entries = std::make_shared<std::vector<HttpTransaction>>();
    auto tab = MakeProxyTab(cfg, entries);
    EXPECT_TRUE(tab != nullptr);
    auto elem = tab->Render();
    EXPECT_TRUE(elem != nullptr);
}

TEST(UIEventsTest, RepeaterTabInstantiation) {
    auto tab = MakeRepeaterTab();
    EXPECT_TRUE(tab != nullptr);
    auto elem = tab->Render();
    EXPECT_TRUE(elem != nullptr);
}

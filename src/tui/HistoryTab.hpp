#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "util/TsQueue.hpp"
#include "http/HttpTransaction.hpp"
#include <memory>
#include <vector>

namespace BurpTUI {

/// History tab with live traffic from the proxy backend.
ftxui::Component MakeHistoryTab(
    std::shared_ptr<TsQueue<HttpTransaction>> txQueue,
    ftxui::ScreenInteractive* screen,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries = nullptr);

} // namespace BurpTUI

#pragma once
#include <ftxui/component/component.hpp>
#include "app/Config.hpp"
#include "http/HttpTransaction.hpp"
#include <memory>
#include <vector>

namespace BurpTUI {

/// Proxy intercept tab: live traffic stream + listener status + intercept controls.
ftxui::Component MakeProxyTab(
    const Config& cfg,
    std::shared_ptr<std::vector<HttpTransaction>> sharedEntries = nullptr);

} // namespace BurpTUI

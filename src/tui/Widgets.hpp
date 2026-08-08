#pragma once
// Shared widget helpers for re-use across tabs.
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <string>

namespace BurpTUI::Widgets {

/// A bordered, titled panel wrapping arbitrary content.
ftxui::Element Panel(const std::string& title, ftxui::Element content);

/// A status-bar element (bottom of screen).
ftxui::Element StatusBar(const std::string& left, const std::string& right);

} // namespace BurpTUI::Widgets

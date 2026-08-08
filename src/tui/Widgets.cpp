#include "tui/Widgets.hpp"
#include <ftxui/dom/elements.hpp>

namespace BurpTUI::Widgets {

using namespace ftxui;

Element Panel(const std::string& title, Element content) {
    return window(text(" " + title + " "), std::move(content));
}

Element StatusBar(const std::string& left, const std::string& right) {
    return hbox({
        text("  " + left) | dim,
        filler(),
        text(right + "  ") | dim,
    }) | size(HEIGHT, EQUAL, 1);
}

} // namespace BurpTUI::Widgets

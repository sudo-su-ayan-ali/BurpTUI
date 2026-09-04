#include "tui/ScrollableViewer.hpp"
#include "tui/HttpFormatter.hpp"
#include "util/Clipboard.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>

namespace BurpTUI {

using namespace ftxui;

ScrollableViewer::ScrollableViewer(std::string title)
    : title_(std::move(title))
{
}

void ScrollableViewer::SetContent(Elements lines, std::string rawText) {
    lines_ = std::move(lines);
    rawText_ = std::move(rawText);
    scroll_y_ = 0;
    statusMsg_.clear();
}

void ScrollableViewer::SetRawText(const std::string& rawText, const std::string& contentType) {
    rawText_ = rawText;
    lines_ = FormatBodyLines(rawText, contentType);
    scroll_y_ = 0;
    statusMsg_.clear();
}

void ScrollableViewer::ScrollTo(int y) {
    scroll_y_ = std::max(0, std::min(max_scroll_, y));
}

void ScrollableViewer::ScrollBy(int delta) {
    ScrollTo(scroll_y_ + delta);
}

bool ScrollableViewer::CopyToClipboard() {
    if (rawText_.empty()) return false;
    bool ok = Clipboard::copy(rawText_);
    if (ok) {
        statusMsg_ = "Copied to clipboard (" + std::to_string(rawText_.size()) + " B)";
    } else {
        statusMsg_ = "Failed to copy";
    }
    return ok;
}

bool ScrollableViewer::OnEvent(Event event) {
    // 1. Mouse Wheel & Click
    if (event.is_mouse()) {
        if (box_.Contain(event.mouse().x, event.mouse().y)) {
            if (event.mouse().button == Mouse::WheelUp) {
                ScrollBy(-3);
                return true;
            }
            if (event.mouse().button == Mouse::WheelDown) {
                ScrollBy(3);
                return true;
            }
            if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Pressed) {
                TakeFocus();
                return true;
            }
        }
        return false;
    }

    // 2. Keyboard (when focused or active)
    if (Focused()) {
        if (event == Event::ArrowUp || event == Event::Character('k')) {
            ScrollBy(-1);
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character('j')) {
            ScrollBy(1);
            return true;
        }
        if (event == Event::PageUp) {
            ScrollBy(-12);
            return true;
        }
        if (event == Event::PageDown) {
            ScrollBy(12);
            return true;
        }
        if (event == Event::Home) {
            ScrollTo(0);
            return true;
        }
        if (event == Event::End) {
            ScrollTo(max_scroll_);
            return true;
        }
        if (event == Event::Character('y') || event == Event::Character('c') || event == Event::Character('C')) {
            CopyToClipboard();
            return true;
        }
    }

    return false;
}

Element ScrollableViewer::Render() {
    // Determine visible line capacity from bounding box
    int h = box_.y_max - box_.y_min + 1;
    int visible_height = std::max(2, h);
    int total_lines = static_cast<int>(lines_.size());
    max_scroll_ = std::max(0, total_lines - visible_height);

    scroll_y_ = std::max(0, std::min(max_scroll_, scroll_y_));

    Elements visible;
    visible.reserve(visible_height);

    for (int i = scroll_y_; i < total_lines && i < scroll_y_ + visible_height; ++i) {
        visible.push_back(lines_[i]);
    }

    if (visible.empty()) {
        visible.push_back(text("(No content)") | dim);
    }

    // Build scroll header / status if scrolled
    std::string posInfo = "[" + std::to_string(scroll_y_ + 1) + "/" + std::to_string(std::max(1, total_lines)) + "]";
    if (!statusMsg_.empty()) {
        posInfo = statusMsg_ + "  " + posInfo;
    }

    auto content = vbox(std::move(visible))
                 | reflect(box_)
                 | vscroll_indicator
                 | frame
                 | flex;

    bool isFocused = Focused();
    std::string focusHint = isFocused ? " [✓ Focused: ↑/↓/PgUp/PgDn to scroll] " : "";

    auto boxElem = vbox({
        content,
        separatorLight(),
        hbox({
            text(" " + (title_.empty() ? "Viewer" : title_)) | (isFocused ? bold | color(Color::GreenLight) : dim),
            text(focusHint) | color(Color::YellowLight),
            filler(),
            text(posInfo + " ") | (isFocused ? color(Color::GreenLight) : dim),
        }) | size(HEIGHT, EQUAL, 1),
    }) | flex;

    if (isFocused) {
        return boxElem | borderStyled(BorderStyle::LIGHT, Color::GreenLight);
    }
    return boxElem;
}

std::shared_ptr<ScrollableViewer> MakeScrollableViewer(std::string title) {
    return std::make_shared<ScrollableViewer>(std::move(title));
}

} // namespace BurpTUI

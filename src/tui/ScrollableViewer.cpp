#include "tui/ScrollableViewer.hpp"
#include "tui/HttpFormatter.hpp"
#include "util/Clipboard.hpp"
#include <ftxui/dom/elements.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace BurpTUI {

using namespace ftxui;

ScrollableViewer::ScrollableViewer(std::string title)
    : title_(std::move(title))
{
}

void ScrollableViewer::updateRawLines() {
    rawLines_.clear();
    std::string clean;
    for (char c : rawText_) {
        if (c != '\r') clean += c;
    }
    std::istringstream stream(clean);
    std::string line;
    while (std::getline(stream, line)) {
        rawLines_.push_back(line);
    }
    if (rawLines_.empty()) {
        rawLines_.push_back("");
    }
}

void ScrollableViewer::SetContent(Elements lines, std::string rawText) {
    lines_ = std::move(lines);
    rawText_ = std::move(rawText);
    updateRawLines();
    scroll_y_ = 0;
    statusMsg_.clear();
    hasSelection_ = false;
    selecting_ = false;
}

void ScrollableViewer::SetRawText(const std::string& rawText, const std::string& contentType) {
    rawText_ = rawText;
    lines_ = FormatBodyLines(rawText, contentType);
    updateRawLines();
    scroll_y_ = 0;
    statusMsg_.clear();
    hasSelection_ = false;
    selecting_ = false;
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
        statusMsg_ = "Copied entire text (" + std::to_string(rawText_.size()) + " B)";
    } else {
        statusMsg_ = "Failed to copy";
    }
    return ok;
}

std::string ScrollableViewer::GetSelectedText() const {
    if (!hasSelection_ || rawLines_.empty()) return "";

    int r1 = selStartRow_, c1 = selStartCol_;
    int r2 = selEndRow_, c2 = selEndCol_;

    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    r1 = std::max(0, std::min(static_cast<int>(rawLines_.size()) - 1, r1));
    r2 = std::max(0, std::min(static_cast<int>(rawLines_.size()) - 1, r2));

    std::ostringstream oss;
    for (int r = r1; r <= r2; ++r) {
        const std::string& line = rawLines_[r];
        int start = (r == r1) ? std::max(0, std::min(static_cast<int>(line.size()), c1)) : 0;
        int end = (r == r2) ? std::max(0, std::min(static_cast<int>(line.size()), c2)) : static_cast<int>(line.size());

        if (start < end) {
            oss << line.substr(start, end - start);
        }
        if (r < r2) {
            oss << "\r\n";
        }
    }
    return oss.str();
}

std::string ScrollableViewer::GetWordAt(int row, int col) const {
    if (row < 0 || row >= static_cast<int>(rawLines_.size())) return "";
    const std::string& line = rawLines_[row];
    if (col < 0 || col >= static_cast<int>(line.size())) return "";

    auto isWordChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/' || c == ':' || c == '=' || c == '&' || c == '?';
    };

    if (!isWordChar(line[col])) return "";

    int start = col;
    while (start > 0 && isWordChar(line[start - 1])) {
        start--;
    }
    int end = col;
    while (end < static_cast<int>(line.size()) && isWordChar(line[end])) {
        end++;
    }
    return line.substr(start, end - start);
}

bool ScrollableViewer::OnEvent(Event event) {
    // 1. Mouse Wheel & Drag Selection & Right-Click
    if (event.is_mouse()) {
        if (box_.Contain(event.mouse().x, event.mouse().y)) {
            int relY = event.mouse().y - box_.y_min;
            int relX = event.mouse().x - box_.x_min;
            int targetRow = scroll_y_ + relY;
            int targetCol = std::max(0, relX);

            if (event.mouse().button == Mouse::WheelUp) {
                ScrollBy(-3);
                return true;
            }
            if (event.mouse().button == Mouse::WheelDown) {
                ScrollBy(3);
                return true;
            }

            // Left Click Pressed -> Begin Selection
            if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Pressed) {
                TakeFocus();
                selStartRow_ = targetRow;
                selStartCol_ = targetCol;
                selEndRow_ = targetRow;
                selEndCol_ = targetCol;
                selecting_ = true;
                hasSelection_ = true;
                return true;
            }

            // Dragging Selection
            if (selecting_ && event.mouse().button == Mouse::Left) {
                selEndRow_ = targetRow;
                selEndCol_ = targetCol;
                return true;
            }

            // Left Click Released -> Finalize Selection & Copy
            if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Released) {
                if (selecting_) {
                    selecting_ = false;
                    selEndRow_ = targetRow;
                    selEndCol_ = targetCol;
                    if (selStartRow_ == selEndRow_ && std::abs(selStartCol_ - selEndCol_) <= 1) {
                        hasSelection_ = false;
                    } else {
                        std::string sel = GetSelectedText();
                        if (!sel.empty()) {
                            Clipboard::copy(sel);
                            statusMsg_ = "Copied (" + std::to_string(sel.size()) + " B)";
                        }
                    }
                }
                return true;
            }

            // Right Click -> Copy Selected Text or Word under cursor
            if (event.mouse().button == Mouse::Right && event.mouse().motion == Mouse::Pressed) {
                TakeFocus();
                if (hasSelection_) {
                    std::string sel = GetSelectedText();
                    if (!sel.empty()) {
                        Clipboard::copy(sel);
                        statusMsg_ = "Copied selection (" + std::to_string(sel.size()) + " B)";
                    }
                } else {
                    std::string word = GetWordAt(targetRow, targetCol);
                    if (!word.empty()) {
                        Clipboard::copy(word);
                        statusMsg_ = "Copied word: " + word;
                    } else {
                        CopyToClipboard();
                    }
                }
                return true;
            }
        } else if (selecting_ && event.mouse().motion == Mouse::Released) {
            selecting_ = false;
            return true;
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
            if (hasSelection_) {
                std::string sel = GetSelectedText();
                Clipboard::copy(sel);
                statusMsg_ = "Copied selection (" + std::to_string(sel.size()) + " B)";
            } else {
                CopyToClipboard();
            }
            return true;
        }
    }

    return false;
}

Element ScrollableViewer::Render() {
    int h = box_.y_max - box_.y_min + 1;
    int visible_height = std::max(2, h - 2);
    int total_lines = static_cast<int>(lines_.size());
    max_scroll_ = std::max(0, total_lines - visible_height);

    scroll_y_ = std::max(0, std::min(max_scroll_, scroll_y_));

    Elements visible;
    visible.reserve(visible_height);

    int r1 = selStartRow_, c1 = selStartCol_;
    int r2 = selEndRow_, c2 = selEndCol_;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    for (int i = scroll_y_; i < total_lines && i < scroll_y_ + visible_height; ++i) {
        if (hasSelection_ && i >= r1 && i <= r2 && i < static_cast<int>(rawLines_.size())) {
            const std::string& raw = rawLines_[i];
            int start = (i == r1) ? std::max(0, std::min(static_cast<int>(raw.size()), c1)) : 0;
            int end = (i == r2) ? std::max(0, std::min(static_cast<int>(raw.size()), c2)) : static_cast<int>(raw.size());

            std::string before = raw.substr(0, start);
            std::string selected = (start < end) ? raw.substr(start, end - start) : "";
            std::string after = (end < static_cast<int>(raw.size())) ? raw.substr(end) : "";

            visible.push_back(hbox({
                text(before),
                text(selected) | bgcolor(Color::CyanLight) | color(Color::Black) | bold,
                text(after),
            }));
        } else {
            visible.push_back(lines_[i]);
        }
    }

    if (visible.empty()) {
        visible.push_back(text("(No content)") | dim);
    }

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
    std::string focusHint = isFocused ? " [✓ Focused: Drag/Right-Click or Arrows to scroll] " : "";

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

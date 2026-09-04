#include "tui/TextEditor.hpp"
#include "util/Clipboard.hpp"
#include <ftxui/dom/elements.hpp>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>

namespace BurpTUI {

using namespace ftxui;

TextEditor::TextEditor(std::string title)
    : title_(std::move(title))
{
    lines_ = {""};
}

void TextEditor::SetText(const std::string& text) {
    lines_.clear();
    std::string clean;
    for (char c : text) {
        if (c != '\r') clean += c;
    }
    std::istringstream stream(clean);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(line);
    }
    if (lines_.empty()) {
        lines_.push_back("");
    }
    cursor_row_ = 0;
    cursor_col_ = 0;
    scroll_y_ = 0;
    statusMsg_.clear();
    hasSelection_ = false;
    selecting_ = false;
}

std::string TextEditor::GetText() const {
    std::ostringstream oss;
    for (size_t i = 0; i < lines_.size(); ++i) {
        oss << lines_[i];
        if (i + 1 < lines_.size()) {
            oss << "\r\n";
        }
    }
    return oss.str();
}

void TextEditor::Clear() {
    lines_ = {""};
    cursor_row_ = 0;
    cursor_col_ = 0;
    scroll_y_ = 0;
    statusMsg_ = "Cleared";
    hasSelection_ = false;
    selecting_ = false;
}

bool TextEditor::CopyToClipboard() {
    std::string text = GetText();
    if (text.empty()) return false;
    bool ok = Clipboard::copy(text);
    if (ok) {
        statusMsg_ = "Copied entire text (" + std::to_string(text.size()) + " B)";
    } else {
        statusMsg_ = "Failed to copy";
    }
    return ok;
}

std::string TextEditor::GetSelectedText() const {
    if (!hasSelection_ || lines_.empty()) return "";

    int r1 = selStartRow_, c1 = selStartCol_;
    int r2 = selEndRow_, c2 = selEndCol_;

    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    r1 = std::max(0, std::min(static_cast<int>(lines_.size()) - 1, r1));
    r2 = std::max(0, std::min(static_cast<int>(lines_.size()) - 1, r2));

    std::ostringstream oss;
    for (int r = r1; r <= r2; ++r) {
        const std::string& line = lines_[r];
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

std::string TextEditor::GetWordAt(int row, int col) const {
    if (row < 0 || row >= static_cast<int>(lines_.size())) return "";
    const std::string& line = lines_[row];
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

void TextEditor::ensureCursorVisible(int visibleHeight) {
    if (visibleHeight <= 0) return;
    if (cursor_row_ < scroll_y_) {
        scroll_y_ = cursor_row_;
    } else if (cursor_row_ >= scroll_y_ + visibleHeight) {
        scroll_y_ = cursor_row_ - visibleHeight + 1;
    }
    int maxScroll = std::max(0, static_cast<int>(lines_.size()) - visibleHeight);
    scroll_y_ = std::max(0, std::min(maxScroll, scroll_y_));
}

void TextEditor::insertString(const std::string& str) {
    for (char c : str) {
        if (c == '\r') continue;
        if (c == '\n') {
            std::string rem = lines_[cursor_row_].substr(cursor_col_);
            lines_[cursor_row_] = lines_[cursor_row_].substr(0, cursor_col_);
            lines_.insert(lines_.begin() + cursor_row_ + 1, rem);
            cursor_row_++;
            cursor_col_ = 0;
        } else {
            lines_[cursor_row_].insert(cursor_col_, 1, c);
            cursor_col_++;
        }
    }
}

bool TextEditor::OnEvent(Event event) {
    int visibleHeight = std::max(2, box_.y_max - box_.y_min - 2);
    int gutterWidth = 6;

    // 1. Mouse Interaction
    if (event.is_mouse()) {
        if (box_.Contain(event.mouse().x, event.mouse().y)) {
            int relY = event.mouse().y - box_.y_min - 1; // account for header border
            int relX = event.mouse().x - box_.x_min - gutterWidth;
            int targetRow = std::max(0, std::min(static_cast<int>(lines_.size()) - 1, scroll_y_ + std::max(0, relY)));
            int targetCol = std::max(0, std::min(static_cast<int>(lines_[targetRow].size()), std::max(0, relX)));

            if (event.mouse().button == Mouse::WheelUp) {
                scroll_y_ = std::max(0, scroll_y_ - 3);
                return true;
            }
            if (event.mouse().button == Mouse::WheelDown) {
                int maxScroll = std::max(0, static_cast<int>(lines_.size()) - visibleHeight);
                scroll_y_ = std::min(maxScroll, scroll_y_ + 3);
                return true;
            }

            // Left Click Pressed -> Set Cursor & Begin Selection
            if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Pressed) {
                TakeFocus();
                cursor_row_ = targetRow;
                cursor_col_ = targetCol;
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
                cursor_row_ = targetRow;
                cursor_col_ = targetCol;
                selEndRow_ = targetRow;
                selEndCol_ = targetCol;
                return true;
            }

            // Left Click Released -> Finalize Selection
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

            // Right Click -> Copy Selection or Word under cursor
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

    if (!Focused()) {
        return false;
    }

    // 2. Keyboard Navigation
    if (event == Event::ArrowUp) {
        if (cursor_row_ > 0) {
            cursor_row_--;
            cursor_col_ = std::min(cursor_col_, static_cast<int>(lines_[cursor_row_].size()));
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }
    if (event == Event::ArrowDown) {
        if (cursor_row_ + 1 < static_cast<int>(lines_.size())) {
            cursor_row_++;
            cursor_col_ = std::min(cursor_col_, static_cast<int>(lines_[cursor_row_].size()));
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }
    if (event == Event::ArrowLeft) {
        if (cursor_col_ > 0) {
            cursor_col_--;
        } else if (cursor_row_ > 0) {
            cursor_row_--;
            cursor_col_ = static_cast<int>(lines_[cursor_row_].size());
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }
    if (event == Event::ArrowRight) {
        if (cursor_col_ < static_cast<int>(lines_[cursor_row_].size())) {
            cursor_col_++;
        } else if (cursor_row_ + 1 < static_cast<int>(lines_.size())) {
            cursor_row_++;
            cursor_col_ = 0;
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }
    if (event == Event::Home) {
        cursor_col_ = 0;
        hasSelection_ = false;
        return true;
    }
    if (event == Event::End) {
        cursor_col_ = static_cast<int>(lines_[cursor_row_].size());
        hasSelection_ = false;
        return true;
    }
    if (event == Event::PageUp) {
        cursor_row_ = std::max(0, cursor_row_ - 10);
        cursor_col_ = std::min(cursor_col_, static_cast<int>(lines_[cursor_row_].size()));
        ensureCursorVisible(visibleHeight);
        hasSelection_ = false;
        return true;
    }
    if (event == Event::PageDown) {
        cursor_row_ = std::min(static_cast<int>(lines_.size()) - 1, cursor_row_ + 10);
        cursor_col_ = std::min(cursor_col_, static_cast<int>(lines_[cursor_row_].size()));
        ensureCursorVisible(visibleHeight);
        hasSelection_ = false;
        return true;
    }

    // 3. Text Modification
    if (event == Event::Return) {
        std::string cur = lines_[cursor_row_];
        std::string left = cur.substr(0, cursor_col_);
        std::string right = cur.substr(cursor_col_);
        lines_[cursor_row_] = left;
        lines_.insert(lines_.begin() + cursor_row_ + 1, right);
        cursor_row_++;
        cursor_col_ = 0;
        ensureCursorVisible(visibleHeight);
        hasSelection_ = false;
        return true;
    }

    if (event == Event::Backspace) {
        if (cursor_col_ > 0) {
            lines_[cursor_row_].erase(cursor_col_ - 1, 1);
            cursor_col_--;
        } else if (cursor_row_ > 0) {
            int prevLen = static_cast<int>(lines_[cursor_row_ - 1].size());
            lines_[cursor_row_ - 1] += lines_[cursor_row_];
            lines_.erase(lines_.begin() + cursor_row_);
            cursor_row_--;
            cursor_col_ = prevLen;
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }

    if (event == Event::Delete) {
        if (cursor_col_ < static_cast<int>(lines_[cursor_row_].size())) {
            lines_[cursor_row_].erase(cursor_col_, 1);
        } else if (cursor_row_ + 1 < static_cast<int>(lines_.size())) {
            lines_[cursor_row_] += lines_[cursor_row_ + 1];
            lines_.erase(lines_.begin() + cursor_row_ + 1);
            ensureCursorVisible(visibleHeight);
        }
        hasSelection_ = false;
        return true;
    }

    if (event == Event::Tab) {
        lines_[cursor_row_].insert(cursor_col_, "    ");
        cursor_col_ += 4;
        hasSelection_ = false;
        return true;
    }

    // Insert character or pasted text
    if (event.is_character()) {
        insertString(event.character());
        ensureCursorVisible(visibleHeight);
        hasSelection_ = false;
        return true;
    }

    return false;
}

Element TextEditor::Render() {
    int h = box_.y_max - box_.y_min + 1;
    int visibleHeight = std::max(2, h - 2);
    ensureCursorVisible(visibleHeight);

    int totalLines = static_cast<int>(lines_.size());
    Elements visibleRows;
    visibleRows.reserve(visibleHeight);

    bool isFocused = Focused();

    int r1 = selStartRow_, c1 = selStartCol_;
    int r2 = selEndRow_, c2 = selEndCol_;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    for (int i = scroll_y_; i < totalLines && i < scroll_y_ + visibleHeight; ++i) {
        const std::string& line = lines_[i];

        std::ostringstream gutterOss;
        gutterOss << std::setw(4) << (i + 1) << " │ ";
        auto gutterElem = text(gutterOss.str()) | dim | color(Color::GrayDark);

        Element lineContent;
        if (hasSelection_ && i >= r1 && i <= r2) {
            int start = (i == r1) ? std::max(0, std::min(static_cast<int>(line.size()), c1)) : 0;
            int end = (i == r2) ? std::max(0, std::min(static_cast<int>(line.size()), c2)) : static_cast<int>(line.size());

            std::string before = line.substr(0, start);
            std::string selected = (start < end) ? line.substr(start, end - start) : "";
            std::string after = (end < static_cast<int>(line.size())) ? line.substr(end) : "";

            lineContent = hbox({
                text(before),
                text(selected) | bgcolor(Color::CyanLight) | color(Color::Black) | bold,
                text(after),
            });
        } else if (isFocused && i == cursor_row_) {
            std::string before = (cursor_col_ <= static_cast<int>(line.size()))
                                     ? line.substr(0, cursor_col_)
                                     : line;
            std::string cursorCh = " ";
            if (cursor_col_ < static_cast<int>(line.size())) {
                cursorCh = std::string(1, line[cursor_col_]);
            }
            std::string after = "";
            if (cursor_col_ + 1 < static_cast<int>(line.size())) {
                after = line.substr(cursor_col_ + 1);
            }

            lineContent = hbox({
                text(before),
                text(cursorCh) | bgcolor(Color::White) | color(Color::Black) | bold,
                text(after),
            });
        } else {
            lineContent = text(line);
        }

        visibleRows.push_back(hbox({gutterElem, lineContent}));
    }

    if (visibleRows.empty()) {
        visibleRows.push_back(text(" (empty) ") | dim);
    }

    std::string posInfo = "[Ln " + std::to_string(cursor_row_ + 1) + ", Col " +
                          std::to_string(cursor_col_ + 1) + " | Total " +
                          std::to_string(totalLines) + " lines]";
    if (!statusMsg_.empty()) {
        posInfo = statusMsg_ + "  " + posInfo;
    }

    auto content = vbox(std::move(visibleRows))
                 | reflect(box_)
                 | frame
                 | flex;

    auto footer = hbox({
        text(" " + (title_.empty() ? "Editor" : title_)) | bold | (isFocused ? color(Color::Cyan) : dim),
        filler(),
        text(posInfo + " ") | dim,
    });

    auto editorBox = vbox({content, separatorLight(), footer});
    if (isFocused) {
        return editorBox | borderStyled(BorderStyle::LIGHT, Color::Cyan);
    } else {
        return editorBox | borderLight;
    }
}

std::shared_ptr<TextEditor> MakeTextEditor(std::string title) {
    return std::make_shared<TextEditor>(std::move(title));
}

} // namespace BurpTUI

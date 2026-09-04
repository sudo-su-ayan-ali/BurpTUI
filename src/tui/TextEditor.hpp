#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <string>
#include <vector>
#include <memory>

namespace BurpTUI {

class TextEditor : public ftxui::ComponentBase {
public:
    explicit TextEditor(std::string title = "Request Editor");
    ~TextEditor() override = default;

    void SetText(const std::string& text);
    std::string GetText() const;
    void Clear();

    bool CopyToClipboard();
    std::string GetSelectedText() const;
    std::string GetWordAt(int row, int col) const;

    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event event) override;
    ftxui::Element Render() override;

    int GetCursorRow() const { return cursor_row_; }
    int GetCursorCol() const { return cursor_col_; }
    int GetTotalLines() const { return static_cast<int>(lines_.size()); }
    const std::string& GetStatus() const { return statusMsg_; }

private:
    void ensureCursorVisible(int visibleHeight);
    void insertString(const std::string& str);

    std::string title_;
    std::vector<std::string> lines_{""};
    int cursor_row_ = 0;
    int cursor_col_ = 0;
    int scroll_y_ = 0;
    ftxui::Box box_;
    std::string statusMsg_;

    // Mouse selection
    bool selecting_ = false;
    bool hasSelection_ = false;
    int selStartRow_ = -1;
    int selStartCol_ = -1;
    int selEndRow_ = -1;
    int selEndCol_ = -1;
};

std::shared_ptr<TextEditor> MakeTextEditor(std::string title = "Request Editor");

} // namespace BurpTUI

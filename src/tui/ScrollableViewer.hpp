#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <string>
#include <vector>
#include <memory>

namespace BurpTUI {

class ScrollableViewer : public ftxui::ComponentBase {
public:
    explicit ScrollableViewer(std::string title = "");
    ~ScrollableViewer() override = default;

    void SetContent(ftxui::Elements lines, std::string rawText = "");
    void SetRawText(const std::string& rawText, const std::string& contentType = "text/plain");
    const std::string& GetRawText() const { return rawText_; }

    void ScrollTo(int y);
    void ScrollBy(int delta);

    bool CopyToClipboard();
    std::string GetSelectedText() const;
    std::string GetWordAt(int row, int col) const;

    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event event) override;
    ftxui::Element Render() override;

    int GetScrollY() const { return scroll_y_; }
    int GetTotalLines() const { return static_cast<int>(lines_.size()); }
    const std::string& GetStatus() const { return statusMsg_; }

private:
    void updateRawLines();

    std::string title_;
    std::string rawText_;
    ftxui::Elements lines_;
    std::vector<std::string> rawLines_;
    int scroll_y_ = 0;
    int max_scroll_ = 0;
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

std::shared_ptr<ScrollableViewer> MakeScrollableViewer(std::string title = "");

} // namespace BurpTUI

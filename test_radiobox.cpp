#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <vector>
#include <string>

int main() {
    int selected = 0;
    std::vector<std::string> modes = {"1", "2"};
    auto radio = ftxui::Radiobox(&modes, &selected);
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    screen.Loop(radio);
    return 0;
}

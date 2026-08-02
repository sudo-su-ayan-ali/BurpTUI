# Phase 1: MVP UI Shell (Week 1)

## Goal
Build a working terminal user interface (TUI) application using **FTXUI** with dummy data. At this stage, no networking components or actual proxy logic are implemented. The focus is entirely on the structure, layout, and interaction of the TUI.

---

## 1. Setup CMake project & integrate FTXUI (vcpkg)

Before writing any C++ code, you need a solid build system. We'll use `vcpkg` for package management and `CMake` as the build system.

### `vcpkg.json`
Create a `vcpkg.json` file in the root of your project to define dependencies. For Phase 1, you only need `ftxui`.

```json
{
  "name": "burptui",
  "version": "0.1.0",
  "dependencies": [
    "ftxui"
  ]
}
```

### `CMakeLists.txt`
Set up your top-level `CMakeLists.txt` to require C++17 (or C++20), find the FTXUI package, and link it to your executable.

```cmake
cmake_minimum_required(VERSION 3.20)
project(burptui CXX)

# Modern C++ is a hard requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FTXUI from vcpkg
find_package(ftxui CONFIG REQUIRED)

# Add your main executable
add_executable(burptui 
    src/main.cpp
    src/tui/TuiApp.cpp
    # Add other source files here as you create them
)

# Link FTXUI libraries
target_link_libraries(burptui PRIVATE
    ftxui::screen 
    ftxui::dom 
    ftxui::component
)
```

### Build Instructions
To build the project:
```bash
# Generate build files, pointing to vcpkg's toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
# Compile
cmake --build build
```

---

## 2. Implement Tab Layout (Proxy, History, Repeater, Decoder)

FTXUI is component-based. You will compose your UI using `Component` (for interactive elements) and `Element` (for drawing). 

To create a tabbed interface, you need a state variable (like `tab_index`) and a `Toggle` component.

### Code Structure

```cpp
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>
#include <string>

using namespace ftxui;

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    int tab_index = 0;
    std::vector<std::string> tab_entries = {
        "Proxy", "History", "Repeater", "Decoder"
    };

    // The tab selection component
    auto tab_selection = Toggle(&tab_entries, &tab_index);

    // Dummy content for each tab
    auto proxy_tab    = Renderer([] { return text("Proxy Tab (Under Construction)") | center; });
    auto history_tab  = Renderer([] { return text("History Tab Placeholder") | center; });
    auto repeater_tab = Renderer([] { return text("Repeater Tab (Under Construction)") | center; });
    auto decoder_tab  = Renderer([] { return text("Decoder Tab (Under Construction)") | center; });

    // The container that switches the displayed component based on tab_index
    auto tab_content = Container::Tab(
        {proxy_tab, history_tab, repeater_tab, decoder_tab},
        &tab_index
    );

    // Layout composition: vertically stack the tab selector and the content
    auto main_container = Container::Vertical({
        tab_selection,
        tab_content,
    });

    // Render logic
    auto renderer = Renderer(main_container, [&] {
        return vbox({
            text("BurpTUI - MVP") | bold | center,
            separator(),
            tab_selection->Render() | center,
            separator(),
            tab_content->Render() | flex,
        }) | border;
    });

    // Start the event loop
    screen.Loop(renderer);
    return 0;
}
```

---

## 3. Build History Tab Dummy List and Split-pane

The History tab is the most complex UI piece in Phase 1. It needs a scrollable list of requests on one side, and a detail view on the other.

### The Dummy Data
Create a simple struct to hold dummy data.
```cpp
struct DummyRequest {
    std::string method;
    std::string path;
    int status;
};
```

### The Component Layout
You need a `Menu` (or `Radiobox`) for the list, and a `Renderer` for the details. 

```cpp
// 1. Create dummy data
std::vector<DummyRequest> requests = {
    {"GET", "/api/users", 200},
    {"POST", "/login", 401},
    {"GET", "/images/logo.png", 200},
};

// 2. Format list entries
std::vector<std::string> list_entries;
for (const auto& req : requests) {
    list_entries.push_back(req.method + " " + req.path);
}

// 3. State variable for selected item
int selected_request = 0;

// 4. Create the interactive menu
auto request_list = Menu(&list_entries, &selected_request);

// 5. Create the detail pane
auto detail_pane = Renderer([&] {
    if (requests.empty()) return text("No requests captured.");
    const auto& req = requests[selected_request];
    
    return vbox({
        text("Request Details") | bold,
        separator(),
        text("Method: " + req.method),
        text("Path: " + req.path),
        text("Status: " + std::to_string(req.status)),
        separator(),
        text("Headers: (dummy data)"),
        text("Host: example.com"),
        text("User-Agent: BurpTUI"),
    }) | border;
});

// 6. Combine them horizontally
auto history_container = Container::Horizontal({
    request_list,
    detail_pane,
});

// 7. Render with specific sizing (split pane)
auto history_renderer = Renderer(history_container, [&] {
    return hbox({
        request_list->Render() | size(WIDTH, LESS_THAN, 30) | border, // Left pane (list)
        detail_pane->Render() | flex,                                // Right pane (details)
    });
});
```
Replace the `history_tab` placeholder in the Step 2 code with this `history_renderer`.

---

## 4. Implement Keyboard Navigation

FTXUI handles standard navigation (Arrow keys, Tab) automatically when components are wrapped in `Container::Vertical` or `Container::Horizontal`. However, custom bindings, like pressing `q` to quit, require `CatchEvent`.

### Adding `CatchEvent`
Wrap your main renderer in a `CatchEvent` component to intercept keystrokes before they reach the UI elements.

```cpp
auto screen = ScreenInteractive::Fullscreen();

// ... setup your tabs and renderer as shown above ...

auto main_renderer_with_keys = CatchEvent(renderer, [&](Event event) {
    // Check if the user pressed 'q' or 'Ctrl+C'
    if (event == Event::Character('q') || event == Event::Escape) {
        screen.Exit(); // Stop the loop and close the application
        return true;   // Event handled
    }
    
    // Check if user pressed TAB to cycle tabs
    if (event == Event::Tab) {
        tab_index = (tab_index + 1) % tab_entries.size();
        return true;
    }
    
    return false; // Let FTXUI handle other events (like arrows for menus)
});

// Start the loop with the key-catching component
screen.Loop(main_renderer_with_keys);
```

### Navigation Rules to Verify:
1. **Up/Down Arrows**: Should move the selection up and down in the History list when the list is focused.
2. **Left/Right Arrows**: Should change the active tab if the tab selector is focused.
3. **Tab Key**: Should shift focus between interactive elements (e.g., from the tab selector to the history list) or cycle tabs depending on how you programmed the `CatchEvent`.
4. **'q' Key**: Should instantly exit the program.

Once you have this Phase 1 shell compiling, running, and responding to keyboard input flawlessly, you are ready to introduce the background Boost.Asio networking in Phase 2.

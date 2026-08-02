# Phase 1: MVP UI Shell (Week 1)

## Goal
Build a fully functional, high-performance Terminal User Interface (TUI) application shell using **FTXUI** with synthetic dummy data. During this phase, no network IO, socket listeners, or proxy intercept engines are introduced. The primary focus is establishing the core `ScreenInteractive` event loop, component tree architecture, layout split-panes, state management, and keyboard event routing.

## Prerequisites
This is the first phase — no prior phases are required. Ensure you have a C++20 compiler (GCC 11+, Clang 13+, or MSVC 2022), CMake 3.20+, and vcpkg installed.

---

## 1. Setup CMake Project & Integrate FTXUI via vcpkg

Setting up a robust, scalable build system is foundational for a multi-tabbed C++20 TUI application.

### Step 1.1: Project Directory Structure & Package Manifest Setup
- Establish a clean layout separating header definitions, implementation files, and third-party dependency configurations.
- Configure a package manifest at the repository root to declare FTXUI as a primary dependency.
- Specify dependency metadata so that the package manager automatically clones, compiles, and exposes FTXUI artifacts (Screen, DOM, Component modules) during the CMake generation step.

### Step 1.2: Build Configuration via CMake
- Declare the minimum CMake requirement to support modern toolchain integration.
- Set C++ standard properties strictly to C++20 to leverage modern language features such as concepts, structured bindings, and lambda improvements.
- Import target configurations from the package manager.
- Define the main binary target and explicitly link against FTXUI component, DOM, and screen modules.

### Step 1.3: Toolchain Bootstrapping & Build Execution
- Invoke CMake generation by referencing the appropriate toolchain file.
- Execute build compilation using standard CMake commands, verifying target linkages execute cleanly without symbol resolution errors across all FTXUI modules.

---

## 2. Implement Tab Layout Architecture (Proxy, History, Repeater, Decoder)

FTXUI utilizes a component-driven framework where components handle interaction states and elements manage layout rendering and decoration.

Note: Additional tabs (Scanner, Intruder) will be introduced in Phase 7. The initial tab set covers the core workflow: Proxy, History, Repeater, and Decoder.

### Step 2.1: State Management for Active Views
- Define an explicit active tab state tracking mechanism using an integer index to represent the current active view.
- Store tab identifier labels in an ordered container to synchronize navigation UI components with active content frames.

### Step 2.2: Tab Navigation Header Component Construction
- Create an interactive toggle control component linked directly to the tab labels array and active index reference.
- Configure the toggle to react to directional input and mouse focus, ensuring active selection indicators update in real time.

### Step 2.3: View Port Container Assembly
- Instantiate dedicated rendering views for each functional tab:
  - **Proxy View**: Renders interception controls and active traffic status indicators.
  - **History View**: Houses the master list view and detail preview pane.
  - **Repeater View**: Provides request editor and response viewer panels.
  - **Decoder View**: Houses text manipulation buffers and encoding/decoding transform controls.
- Wrap these individual views inside a tab container component linked directly to the active index variable. This guarantees that only the element corresponding to the selected index is evaluated and drawn on each frame loop.

### Step 2.4: Root Viewport Composition & Styling
- Combine the top tab navigation bar and the main tab content container inside a vertical layout container.
- Construct a root renderer that wraps the container tree in visually distinctive structural borders:
  - Top header displaying application title and versioning.
  - Horizontally centered tab selection bar delimited by visual separators.
  - Flexible body area that dynamically expands to fill all remaining terminal rows and columns.

---

## 3. Build History Tab Dummy List and Split-Pane Layout

The History tab forms the central focal point of the TUI shell, requiring a responsive master-detail layout.

### Step 3.1: Synthetic Data Model Modeling
- Formulate a lightweight domain model representing an `HttpTransaction` (containing fields for transaction ID, HTTP verb, request path, status code, content length, and payload body headers).
- Populate a central memory repository with pre-configured synthetic `HttpTransaction` entries to simulate real-world web application traffic.

### Step 3.2: List View & Menu Control Integration
- Map the synthetic data collection into display string representations formatted for rapid terminal reading (e.g., HTTP method aligned with path and status).
- Maintain a state variable representing the currently highlighted list item.
- Instantiate an interactive menu component backed by the formatted string collection and selection index variable, ensuring vertical list scrolling and item focus functions out of the box.

### Step 3.3: Detail Inspector Pane Rendering
- Build a dedicated detail view renderer that inspects the currently selected `HttpTransaction` index from the synthetic collection.
- Structure the detail view output dynamically using vertical box structures:
  - Inspection header showing transaction metadata.
  - Formatted request section detailing HTTP headers and target parameters.
  - Formatted response section displaying HTTP status messages, headers, and body previews.
  - Fallback empty state rendering when no transactions are available.

### Step 3.4: Dual-Pane Master-Detail Container Assembly
- Combine the list menu component and detail renderer inside a horizontal component container.
- Apply structural layout constraints using flexible DOM modifiers:
  - Constrain the left request list menu to a fixed maximum width fraction with explicit structural borders.
  - Assign flex sizing to the right detail pane, permitting it to automatically claim all remaining terminal width.
  - Apply scrollbar indicators or clear visual split dividers separating master and detail panes.

---

## 4. Implement Global & Contextual Keyboard Navigation

Intuitive keyboard navigation is essential for security tool workflows, minimizing friction when navigating dense HTTP data.

### Step 4.1: Top-Level Event Interception Filter
- Wrap the primary application container in an event handler component to intercept raw keyboard and terminal events before they reach focused components.
- Establish priority handling rules for global action keys:
  - Global application termination to trigger `ScreenInteractive` exit callbacks cleanly.
  - View cycling shortcuts to increment/decrement active view indices modulo the total view count.

### Step 4.2: Focus Management & Panel Switching Strategy
- Configure focus traversal boundaries so pressing directional keys or tab keys moves focus predictably between the header tab selector, the History request list, and detail inspectors.
- Implement explicit sub-component key bindings within focused containers (e.g., standard navigation inside list menus, scrolling inside detail text viewers).

### Step 4.3: Terminal Resize & Boundary Handling Verification
- Verify that terminal resize events automatically refresh element dimensions and recalculate split-pane widths without layout corruption or text overflow artifacts.
- Validate that all input events return boolean flags accurately indicating whether the key press was consumed globally or passed through to child components.

---

## Cross-Cutting Concerns

### Error Handling Strategy
Establish a consistent error reporting pattern from the start. Utilize a status bar or dedicated notification area within the FTXUI layout to present user-facing errors smoothly without breaking the visual interface.

### Logging
Set up a simple file-based debug logger for development troubleshooting. Ensure that UI rendering logic and internal state changes can be traced asynchronously, directing log output to a file rather than the active terminal console to prevent visual corruption.

### Configuration
Make the application configurable by externalizing the proxy listen port, database path, and CA directory. Support providing these values via command-line arguments or a structured configuration file parsed at startup.

### Graceful Shutdown
Ensure the application cleanly exits when the user presses the designated quit keys. This involves triggering the `ScreenInteractive` exit functions, draining any active `TsQueue` instances, and properly destroying all FTXUI components to restore the terminal to its original state.

---

## Completion Checklist

- [ ] CMake project compiles without errors with FTXUI linked
- [ ] Application launches in fullscreen terminal mode
- [ ] All four tabs (Proxy, History, Repeater, Decoder) are visible and switchable
- [ ] History tab displays a scrollable dummy request list with a split-pane detail view
- [ ] Arrow keys navigate the list, Tab cycles views, and 'q' exits the application
- [ ] Terminal resize events are handled without layout corruption

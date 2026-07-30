# UI/UX Wireframes & Component Specifications: BurpTUI

## 1. UI/UX Overview
BurpTUI utilizes FTXUI to render a modern, responsive, and composable terminal user interface. The interface relies strictly on keyboard navigation (and terminal mouse support where available) without standard GUI elements. 

## 2. Global Layout & Shell
The top-level shell consists of a persistent Tab bar for navigation, with the main body content changing based on the selected tab.

```text
┌─────────────────────────────────────────────────────────────┐
│ BurpTUI v0.1.0  [ Proxy ] [ History ] [ Repeater ] [ Decoder ]
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                                                             │
│                      (Tab Content Area)                     │
│                                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```
**Global Keybindings:**
- `Tab` or `Shift+Tab`: Cycle through tabs.
- `Left`/`Right` Arrows (when tab bar is focused): Switch tabs.
- `Ctrl+C` or `q`: Gracefully quit the application.

## 3. Tab Component Specifications

### 3.1 Proxy Tab
**Purpose:** Provides a dashboard for the proxy status, intercept toggles, and live traffic metrics.
**Components:**
- `Toggle`: "Intercept Mode" (On / Off). Default Off.
- `Text`: Displays proxy listening port (e.g., `Listening on :8080`).
- `Gauges / Counters`: Total requests captured, TLS vs HTTP ratio.

### 3.2 History Tab
**Purpose:** The core viewing pane for all intercepted traffic.
**Layout:** Horizontally or vertically split pane.
- **Left/Top Pane:** A scrollable list/table of `TrafficEvent`s showing Method, Host, Path, Status Code, and Time.
- **Right/Bottom Pane:** Detail view of the selected request. It is vertically split itself:
  - Top half: Request Headers & Body
  - Bottom half: Response Headers & Body
**Interactions:**
- `Up`/`Down` Arrows: Select requests.
- `Enter`: Focus detail pane.
- `r` or `Ctrl+R`: Send highlighted request to the Repeater Tab.

### 3.3 Repeater Tab
**Purpose:** Edit and replay captured requests.
**Layout:** 
- **Top Bar:** Input field for Target Host & Port (e.g., `api.example.com:443`), and a "Send" Button.
- **Left Pane (Request Editor):** Multiline `Input` component containing the raw request string (Headers + Body). Fully editable.
- **Right Pane (Response Viewer):** Read-only text area displaying the server's response.
**Interactions:**
- Users can modify the raw HTTP text.
- Pressing `Send` triggers an asynchronous HTTP connection. The TUI remains responsive until the response populates the right pane.

### 3.4 Decoder Tab
**Purpose:** Utility for encoding and decoding strings (Base64, URL, Hex).
**Layout:**
- **Input Area:** Multiline text input.
- **Action Bar:** Radio buttons or drop-down for format selection (Base64, URL Encode/Decode). Encode/Decode buttons.
- **Output Area:** Read-only output box.

## 4. TUI Component State Transitions & Rendering
- **Reactivity:** FTXUI components re-render automatically when their internal state changes, provided `ScreenInteractive::PostEvent` is called.
- **Modals:** Fatal errors (e.g., port in use) should trigger an FTXUI modal overlaying the entire screen before shutting down.
- **Truncation:** Long strings (like large request bodies) will be truncated to fit the terminal window bounds, with an indicator like `... [Truncated]`. Hex viewer components will be used for binary bodies.

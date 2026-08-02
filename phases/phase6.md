# Phase 6: Extended Features (Weeks 9–11)

## Goal
The goal of this phase is to elevate the proxy from a passive monitoring and replay tool into an active, inline manipulation engine. This involves implementing an Intercept Mode to halt traffic in transit for manual modification, and building a Decoder Utility tab to assist the user in analyzing and manipulating obfuscated or encoded payloads.

---

## 1. Intercept Mode (Pause, Edit, Forward/Drop)

Intercept Mode acts as a breakpoint for HTTP traffic. When enabled, incoming client requests and outgoing server responses are held in memory, allowing the user to inspect and edit them before deciding their fate.

### Step 1.1: State and Intercept Configuration
- Create a global boolean state variable (e.g., `intercept_is_on`) that is toggleable from the TUI (typically located within the Proxy tab).
- Enhance the `MitmSession` handler logic to check this state flag immediately after fully parsing an incoming HTTP request (but before writing it to the upstream socket) and after parsing a response (before writing it to the client socket).
- If the flag is false, the session continues to stream data normally. If true, the session enters a suspended state, halting any further socket reads or writes for that specific connection and waiting for user input.

### Step 1.2: The Interception Queue and UI Notification
- When a session suspends, construct an `InterceptedTransaction` object containing the raw request/response string, a unique session identifier, and the direction of the traffic (client-to-server or server-to-client).
- Push this object into a thread-safe "Intercept Queue."
- Dispatch a custom FTXUI event to the main UI thread to notify it that traffic has been caught.
- The UI should react by switching focus to the Proxy Intercept tab and displaying the raw HTTP text of the first item in the Intercept Queue inside a multiline text editor.

### Step 1.3: Forward and Drop Mechanisms
- Introduce three primary action buttons in the Intercept UI: "Forward", "Drop", and "Intercept is On/Off".
- **Forward**: When the user clicks Forward (or presses a hotkey), extract the potentially modified text from the editor. Signal the suspended `MitmSession` (via a thread-safe condition variable or promise/future mechanism) to wake up. Replace the session's internal buffer with the modified text, and proceed to write it to the destination socket.
- **Drop**: When the user clicks Drop, signal the suspended `MitmSession` to abort. The session should immediately close both the client and server sockets and cleanly destruct itself, effectively killing the connection without forwarding the data.
- Ensure the UI gracefully pops the handled transaction from the queue and immediately loads the next pending transaction, if any exist.

---

## 2. Decoder Utility (Base64, URL, Hex, HTML Entities)

The Decoder tab is an offline sandbox allowing the user to transform strings rapidly without needing external tools.

### Step 2.1: Decoder UI Layout
- Structure the Decoder tab using a vertically stacked layout (`Container::Vertical`).
- The layout should consist of a primary multiline input editor at the top for the raw input string.
- Below the input, arrange a horizontal row of buttons or dropdown menus corresponding to different encoding/decoding operations (e.g., "Decode as Base64", "Encode as URL", "Hex to ASCII").
- At the bottom, place a read-only multiline text viewer to display the result of the transformation.

### Step 2.2: Implement Transformation Algorithms
- Create a stateless utility library (`TransformUtils`) containing purely functional routines for standard web encodings:
  - **Base64**: Implement standard Base64 encoding and decoding algorithms, properly handling padding characters (`=`).
  - **URL Encoding**: Implement percent-encoding/decoding for reserved URL characters (`%20`, `%3D`, etc.).
  - **Hex**: Implement routines to convert ASCII strings to hexadecimal representations and vice-versa.
  - **HTML Entities**: Implement mapping routines to decode standard HTML entities (e.g., `&lt;` to `<`) and encode special characters.
- Ensure these functions are robust against malformed input. For instance, attempting to decode a non-Base64 string as Base64 should return a graceful error string or simply mirror the original input, rather than causing a crash or memory corruption.

### Step 2.3: Wiring the UI to the Utilities
- Bind the action buttons in the UI to their respective utility functions.
- When an operation is triggered, extract the active string from the top input editor.
- Pass the string through the selected transformation function.
- Update the state variable bound to the bottom result viewer with the transformed output string, and trigger a UI re-render.
- **Smart Decoding (Optional Enhancement)**: Implement a "Smart Decode" feature that uses basic heuristics (like checking for `%` symbols or `==` padding) to guess the encoding and automatically apply the correct transformation chain.

By completing Phase 6, the proxy achieves feature parity with standard security testing tools, providing powerful inline manipulation and essential payload analysis utilities.

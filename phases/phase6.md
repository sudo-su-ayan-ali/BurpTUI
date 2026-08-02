# Phase 6: Extended Features (Weeks 9–11)

## Goal
The goal of this phase is to elevate the proxy from a passive monitoring and replay tool into an active, inline manipulation engine. This involves implementing an Intercept Mode to halt traffic in transit for manual modification, and building a Decoder Utility tab to assist the user in analyzing and manipulating obfuscated or encoded payloads.

## Prerequisites
Depends on Phase 5. The Repeater must be functional, and the proxy pipeline (ProxyServer → Session/MitmSession → TsQueue → SqliteStore) must be stable.

---

## 1. Intercept Mode (Pause, Edit, Forward/Drop)

Intercept Mode acts as a breakpoint for HTTP traffic. When enabled, incoming client requests and outgoing server responses are held in memory, allowing the user to inspect and edit them before deciding their fate.

### Step 1.1: State and Intercept Configuration
- Create a global state variable representing the intercept toggle that is controllable from the TUI.
- The intercept toggle state should be visually prominent in the Proxy tab header, using color indicators (e.g., red for active interception, green for pass-through).
- Enhance the MitmSession handler logic to check this state flag immediately after fully parsing an incoming HTTP request and after parsing a response.
- If the flag is false, the session continues to stream data normally. If true, the session enters a suspended state, halting any further socket reads or writes for that specific connection and waiting for user input.

### Step 1.2: The Interception Queue and UI Notification
- When a session suspends, construct an InterceptedTransaction object containing the raw request/response string, a unique session identifier, and the direction of the traffic.
- Push this object into a thread-safe intercept queue.
- Dispatch a custom event to the main UI thread to notify it that traffic has been caught.
- The UI should react by switching focus to the Proxy Intercept tab and displaying the raw HTTP text of the first item in the queue inside a multiline text editor.

### Step 1.3: Forward and Drop Mechanisms
- Introduce primary action controls in the Intercept UI for Forward, Drop, and toggling interception state.
- **Forward**: When the user triggers Forward, extract the potentially modified text from the editor. Signal the suspended MitmSession to wake up, replace the internal buffer with the modified text, and proceed to write it to the destination socket.
- **Drop**: When the user triggers Drop, signal the suspended MitmSession to abort. The session should immediately close the sockets and cleanly destruct itself, terminating the connection without forwarding the data.
- When intercepting traffic on a keep-alive connection, multiple requests may be queued from the same socket. The interception engine must handle these sequentially, holding subsequent requests in a per-session buffer until the current intercepted request is forwarded or dropped.
- Ensure the UI gracefully pops the handled transaction from the queue and immediately loads the next pending InterceptedTransaction, if any exist.

---

## 2. Decoder Utility (Base64, URL, Hex, HTML Entities)

The Decoder tab is an offline sandbox allowing the user to transform strings rapidly without needing external tools.

### Step 2.1: Decoder UI Layout
- Structure the Decoder tab using a vertically stacked layout.
- The layout should consist of a primary multiline input editor at the top for the raw input string.
- Below the input, arrange controls corresponding to different encoding and decoding operations.
- At the bottom, place a read-only multiline text viewer to display the result of the transformation.

### Step 2.2: Implement Transformation Algorithms
- Create a stateless utility module containing purely functional routines for standard web encodings:
  - **Base64**: Implement standard Base64 encoding and decoding algorithms, handling padding correctly.
  - **URL Encoding**: Implement percent-encoding/decoding for reserved URL characters.
  - **Hex**: Implement routines to convert ASCII strings to hexadecimal representations and vice-versa.
  - **HTML Entities**: Implement mapping routines to decode standard HTML entities and encode special characters.
- Ensure these functions are robust against malformed input. Attempting to process invalid input must yield a graceful error message or safely mirror the input without causing instability.

### Step 2.3: Wiring the UI to the Utilities
- Bind the action controls in the UI to their respective utility functions.
- When an operation is triggered, extract the active string from the top input editor.
- Pass the string through the selected transformation function.
- Update the state variable bound to the bottom result viewer with the transformed output string, and trigger a UI re-render.

---

## Cross-Cutting Concerns
- Monitor and optimize the performance impact of Intercept Mode on high-traffic sessions, ensuring that suspended connections do not exhaust system resources or block asynchronous processing loops.

## Completion Checklist
- [ ] Intercept toggle pauses traffic flow when enabled
- [ ] Intercepted requests are displayed in an editable text view
- [ ] Forward action sends the (potentially modified) request to the upstream server
- [ ] Drop action terminates the connection without forwarding
- [ ] Decoder tab correctly encodes/decodes Base64, URL, Hex, and HTML entities
- [ ] Malformed input to the Decoder produces graceful error messages

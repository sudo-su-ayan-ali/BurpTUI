# Phase 5: Repeater (Week 8)

## Goal
The goal of this phase is to implement the Repeater functionality, allowing users to take an intercepted request from the History tab, manually modify its headers, path, or body in a text editor, and replay it against the target server to analyze the corresponding response.

## Prerequisites
Depends on Phase 4. The SqliteStore must be operational, and the History tab must be displaying live, persisted traffic.

---

## 1. Add "Send to Repeater" Hotkey

To bridge the History tab with the Repeater tab, you need an intuitive mechanism to transfer request data between the two contexts without breaking the user's operational flow.

### Step 1.1: Keyboard Event Interception in History Tab
- Within the History tab's layout component, introduce an event listener specifically scoped to the request list menu.
- Define a dedicated hotkey bound to the "Send to Repeater" action.
- Ensure this hotkey only triggers when a specific HttpTransaction in the History list is actively highlighted or selected.

### Step 1.2: State Transfer Mechanism
- Upon triggering the hotkey, query the underlying SqliteStore using the selected HttpTransaction's ID to fetch the complete, raw HTTP request string, including the initial request line, all headers, and the payload body.
- Create a global or shared state object designed to hold Repeater Tasks. Push this raw request string into the Repeater state.
- Automatically update the application's main tab index state to switch focus immediately from the History tab to the Repeater tab, providing immediate visual feedback to the user.

---

## 2. Build Multiline Request Editor

The Repeater requires a robust text editing interface allowing the user to seamlessly modify the raw HTTP request before replaying it.

### Step 2.1: Input Component Integration
- In the Repeater tab's UI layout, instantiate a text input component tailored for multiline support that handles raw string manipulation, cursor positioning, and vertical scrolling.
- Bind this text editor component to the shared state variable populated in the previous step.
- When the tab loads, the editor should visually render the raw, unadulterated HTTP request, ready for manual modification.

### Step 2.2: Editor UI Layout and Controls
- Structure the Repeater view using a split-pane layout. Place the multiline request editor on the left side.
- Reserve the right side of the layout for a read-only text viewer component that will display the server's response.
- Add an interactive action button or bind a dedicated hotkey near the editor. This element will serve as the trigger to dispatch the modified request.

---

## 3. Implement Custom Request Dispatch & Response View

Once the user has modified the request, the application must establish a new network connection, dispatch the raw text as an HTTP payload, and capture the result.

### Step 3.1: Network Dispatch Client
- Create a standalone HTTP client routine. Unlike the proxy sessions, this client does not need to accept incoming connections; it only needs to initiate outbound connections.
- When the user triggers the action to send, extract the active string from the multiline editor.
- Implement a robust parser to extract the target Host header and the port directly from the modified raw text.
- If the target host requires HTTPS (port 443 or explicit scheme), the dispatch client must perform a TLS handshake using a standard client SSL context (not the MITM context) before sending the request.
- Resolve the target host, establish the connection, and asynchronously write the entire raw string buffer directly to the socket.

### Step 3.2: Awaiting and Parsing the Response
- After dispatching the request, immediately initiate an asynchronous read loop on the socket to capture the server's response.
- Feed the incoming byte stream through a response parser instance to ensure the response is structurally valid and to accurately determine when the complete payload has been received.
- Aggregate the parsed headers and body into a single, formatted response string block.

### Step 3.3: Updating the Response View
- Safely marshal the formatted response string from the background networking thread back to the main UI thread utilizing the thread-safe TsQueue.
- Update the state variable bound to the Repeater's response viewer pane.
- Trigger a UI re-render to display the freshly fetched response text to the user, allowing them to compare their modified request against the server's actual behavior.

---

## Cross-Cutting Concerns
- Error handling must gracefully manage network timeouts, connection refused scenarios, and malformed requests originating from the manual editor. Appropriate error messages should be prominently displayed in the response pane.

## Completion Checklist
- [ ] 'Send to Repeater' hotkey transfers the selected request from History
- [ ] Multiline editor displays and allows modification of the raw HTTP request
- [ ] Modified requests are dispatched to the target server
- [ ] Server responses are displayed in the response viewer pane
- [ ] Both HTTP and HTTPS targets are supported
- [ ] Network errors are displayed gracefully in the response pane

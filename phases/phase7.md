# Phase 7: Passive Scanner & Rules (Week 12+)

## Goal
The goal of this phase is to automate the discovery of low-hanging fruit and common misconfigurations by introducing a passive scanning engine, an Intruder tool for payload fuzzing, and advanced protocol support (WebSockets). This transitions the application into a comprehensive vulnerability hunting platform.

## Prerequisites
Depends on Phase 6. All proxy, storage, and UI features from prior phases must be stable and performant.

---

## 1. Passive Scanner Engine

The passive scanner operates silently in the background, analyzing all traffic that passes through the proxy without sending any additional active requests to the target server.

### Step 1.1: Rule Definition Framework
- Create a modular rule engine architecture where individual security checks can be defined independently.
- Each rule should accept a fully parsed HTTP request and response pair as its input and return a standardized ScanFinding object containing severity, title, description, and the specific evidence string if a vulnerability is detected.

### Step 1.2: Header Analysis Rules
- Implement a suite of rules specifically targeting HTTP response headers.
- **Security Headers**: Check the response headers to ensure critical security flags are present, such as strict transport security and content security policies.
- **Cookie Security**: Inspect any session-setting headers originating from the server. Flag cookies that are missing secure or HTTP-only attributes.
- **Information Disclosure**: Scan headers that excessively leak backend version information or operational details.

### Step 1.3: Reflected Input Detection (XSS Sinks)
- Implement a more complex rule to identify potential Cross-Site Scripting vulnerabilities via reflection.
- Extract all user-supplied input from the HTTP request parameters and specific headers.
- Perform a highly performant string search to check if any of those exact input strings are mirrored back in the HTTP response body.
- If reflection is detected, specifically evaluate the response content type and check whether the reflected input appears to be properly escaped, generating a ScanFinding if it appears unsafe.

### Step 1.4: False Positive Management
- Implement a mechanism to mark findings as false positives. Store this flag in the SqliteStore so that dismissed findings do not reappear on subsequent scans. Provide a UI toggle in the Findings tab to show/hide dismissed items.

---

## 2. Scanner/Findings Tab UI

To surface the vulnerabilities discovered by the passive engine, a dedicated interface is required.

### Step 2.1: Findings Data Store
- Enhance the SqliteStore with a dedicated schema to persist vulnerability discoveries. This structured storage should link to the original request/response ID and safely retain all ScanFinding metadata.
- When the background passive scanner generates a ScanFinding, asynchronously write it to the SqliteStore and emit a custom UI update event.

### Step 2.2: Findings UI Layout
- Create a new Findings tab in the main layout.
- Structure it as a split-pane interface.
- The left pane should feature a scrollable list grouping findings by target host or by severity level.
- The right pane should display the detail view: the vulnerability description, remediation advice, and the exact HTTP request/response with the vulnerable payload visually emphasized.

---

## 3. Intruder Tool (Payload Iteration)

The Intruder is an active scanning tool used to fuzz endpoints by rapidly iterating payloads through specific insertion points in a base request.

### Step 3.1: Payload Positioning UI
- Create a new Intruder tab. Allow the user to forward requests here from the History or Repeater environments.
- Implement an interface to define insertion points within the raw request text, wrapping target parameters in distinct visual markers.

### Step 3.2: Attack Types and Payload Generators
- Implement payload generation logic. Begin with a simple iteration mode that systematically replaces one insertion point at a time from a predefined sequence of values.
- Allow the user to load external text files from disk to use as the payload source.

### Step 3.3: Concurrent Execution Engine
- Build an asynchronous execution pool to dispatch the fuzzed requests concurrently, ensuring the UI remains responsive.
- Implement configurable request-per-second throttling to avoid overwhelming target servers or triggering WAF rate limits.
- Store the results in a dedicated in-memory structure or temporary database schema.
- Present these results in a data grid within the Intruder tab, allowing the user to sort by status code, response length, or response time to quickly identify anomalies.

---

## 4. WebSocket Frame Inspection

Modern web applications frequently use WebSockets for real-time, bidirectional communication, bypassing standard HTTP after the initial handshake.

### Step 4.1: Intercepting the Upgrade Handshake
- Enhance the HTTP parser logic to detect the upgrade headers in client requests and the corresponding switching protocols response from the server.
- Once this handshake is detected and successfully negotiated, gracefully transition the connection handling to a specialized protocol stream.

### Step 4.2: Parsing WebSocket Frames
- Implement a custom binary parser that adheres to the established WebSocket standard.
- The parser must read the frame headers to determine the opcode, payload length, and masking keys.
- Safely unmask the payload data coming from the client, and properly parse the unmasked data coming from the server.

### Step 4.3: WebSocket UI Integration
- Create a dedicated sub-view or an entirely new tab for WebSocket traffic.
- Display the parsed frames in a chronological sequence, distinguishing visually between client-to-server and server-to-client messages.
- Allow the user to inspect the unmasked payload in a detail pane. For binary WebSocket frames, display the payload as a hex dump in the detail pane, with an option to decode as UTF-8 if the content appears to be text.

---

## Cross-Cutting Concerns
- Ensure performance under high scanner load remains optimal. Background scanning processes must be deeply decoupled from the main UI thread.
- Uphold security ethics by displaying a persistent reminder urging users to only test systems and infrastructure they have explicit authorization to assess.

## Completion Checklist
- [ ] Passive scanner runs automatically on all proxied traffic
- [ ] Missing security headers are flagged with appropriate severity
- [ ] Reflected input is detected and reported as potential XSS
- [ ] Findings are displayed in a dedicated Scanner/Findings tab
- [ ] False positives can be dismissed and persist across sessions
- [ ] Intruder iterates payloads and displays results in a sortable grid
- [ ] WebSocket frames are captured, unmasked, and displayed chronologically

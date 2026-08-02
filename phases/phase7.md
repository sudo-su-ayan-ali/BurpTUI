# Phase 7: Passive Scanner & Rules (Week 12+)

## Goal
The goal of this phase is to automate the discovery of low-hanging fruit and common misconfigurations by introducing a passive scanning engine, an Intruder tool for payload fuzzing, and advanced protocol support (WebSockets). This transitions the application into a comprehensive vulnerability hunting platform.

---

## 1. Passive Scanner Engine

The passive scanner operates silently in the background, analyzing all traffic that passes through the proxy without sending any additional active requests to the target server.

### Step 1.1: Rule Definition Framework
- Create a modular rule engine architecture where individual security checks can be defined as independent functions or classes.
- Each rule should accept a fully parsed HTTP request and response pair as its input and return a standardized `Finding` object (containing severity, title, description, and the specific evidence string) if a vulnerability is detected.

### Step 1.2: Header Analysis Rules
- Implement a suite of rules specifically targeting HTTP response headers.
- **Security Headers**: Check the response headers to ensure critical security flags are present. For example, verify the presence of `Strict-Transport-Security` (HSTS), `X-Content-Type-Options: nosniff`, and `Content-Security-Policy` (CSP).
- **Cookie Security**: Inspect any `Set-Cookie` headers originating from the server. Flag cookies that are missing the `Secure` or `HttpOnly` attributes.
- **Information Disclosure**: Scan headers like `Server` or `X-Powered-By` that excessively leak backend version information (e.g., `Server: Apache/2.4.1 (Ubuntu)`).

### Step 1.3: Reflected Input Detection (XSS Sinks)
- Implement a more complex rule to identify potential Cross-Site Scripting (XSS) vulnerabilities via reflection.
- Extract all user-supplied input from the HTTP request (e.g., URL query parameters, POST body parameters, and specific headers).
- Perform a fast string search (or use a performant algorithm like Aho-Corasick) to check if any of those exact input strings are mirrored back in the HTTP response body.
- If reflection is detected, specifically check the response `Content-Type` (e.g., `text/html`) and whether the reflected input is properly escaped. Generate a finding if it appears unsafe.

---

## 2. Scanner/Findings Tab UI

To surface the vulnerabilities discovered by the passive engine, a dedicated interface is required.

### Step 2.1: Findings Data Store
- Enhance the `SqliteStore` created in Phase 4 with a new `findings` table. This table should link to the original request/response ID and store the finding metadata (severity, rule name, evidence).
- When the background passive scanner generates a `Finding`, asynchronously write it to this database table and emit a custom UI update event.

### Step 2.2: Findings UI Layout
- Create a new "Scanner" or "Findings" tab in the FTXUI layout.
- Structure it as a split-pane interface similar to the History tab.
- The left pane should feature a scrollable list or tree-view grouping findings by target host or by severity level (High, Medium, Low, Informational).
- The right pane should display the detail view: the vulnerability description, remediation advice, and the exact HTTP request/response with the vulnerable payload visually highlighted (using FTXUI color attributes).

---

## 3. Intruder Tool (Payload Iteration)

The Intruder is an active scanning tool used to fuzz endpoints by rapidly iterating payloads through specific insertion points in a base request.

### Step 3.1: Payload Positioning UI
- Create a new "Intruder" tab. Allow the user to send requests here from the History or Repeater tabs.
- Implement an interface to mark "insertion points" (or payloads) within the raw request text. This could involve wrapping target parameters in special marker characters (e.g., `user=§admin§`).

### Step 3.2: Attack Types and Payload Generators
- Implement payload generation logic. Begin with a simple "Sniper" attack mode, which iterates through a predefined list of strings (a wordlist), replacing one insertion point at a time.
- Allow the user to load external text files (wordlists) from disk to use as the payload source.

### Step 3.3: Concurrent Execution Engine
- Build an asynchronous execution pool using Boost.Asio to dispatch the fuzzed requests concurrently, ensuring the UI remains responsive.
- Store the results (the modified request and the server's corresponding response) in a dedicated in-memory table or temporary database.
- Present these results in a data grid within the Intruder tab, allowing the user to sort by status code, response length, or response time to quickly identify anomalies (e.g., a SQL injection payload that returns a 500 error or a significantly different response length compared to the baseline).

---

## 4. WebSocket Frame Inspection

Modern web applications frequently use WebSockets for real-time, bidirectional communication, bypassing standard HTTP after the initial handshake.

### Step 4.1: Intercepting the Upgrade Handshake
- Enhance the HTTP parser in the `MitmSession` to detect the `Upgrade: websocket` header in client requests and the corresponding `101 Switching Protocols` response from the server.
- Once this handshake is detected and successfully negotiated, detach the standard `llhttp` HTTP parsers from the socket stream.

### Step 4.2: Parsing WebSocket Frames
- Implement a custom binary parser that adheres to RFC 6455 (The WebSocket Protocol).
- The parser must read the WebSocket frame headers to determine the opcode (Text, Binary, Ping, Pong, Close), the payload length, and the masking key.
- Safely unmask the payload data coming from the client, and properly parse the unmasked data coming from the server.

### Step 4.3: WebSocket UI Integration
- Create a dedicated sub-view or an entirely new tab for WebSocket traffic.
- Display the parsed frames in a chronological list, distinguishing visually between client-to-server (e.g., green text) and server-to-client (e.g., blue text) messages.
- Allow the user to click on individual frames to inspect the unmasked payload (whether JSON text or raw binary hex) in a detail pane.

By finalizing Phase 7, the proxy becomes a robust, fully-featured application security testing suite capable of discovering vulnerabilities automatically, actively fuzzing endpoints, and analyzing complex modern protocols.

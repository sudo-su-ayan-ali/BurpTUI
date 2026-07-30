# Project Roadmap & Milestones: BurpTUI

## 1. Overview
The development of BurpTUI is phased incrementally. Because the project heavily intertwines complex asynchronous networking with a terminal UI, each phase builds upon the absolute stability of the previous one. 

## 2. Milestone Timeline

### Phase 1: MVP UI Shell (Week 1)
**Goal:** A working FTXUI application with four tabs and dummy data. No networking components yet.
- **M1.1:** Setup CMake project and integrate FTXUI via `vcpkg`.
- **M1.2:** Implement tab layout (Proxy, History, Repeater, Decoder).
- **M1.3:** Build dummy scrollable list in the History tab and a split-pane detail view.
- **M1.4:** Wire up keyboard navigation (`Tab`, Arrow keys, `q` to quit).

### Phase 2: Plain HTTP Proxy (Weeks 2–3)
**Goal:** Intercept and view real HTTP traffic.
- **M2.1:** Integrate `Boost.Asio` and `llhttp`.
- **M2.2:** Build `ProxyServer` TCP acceptor and `Session` handler.
- **M2.3:** Implement `EventQueue` to push traffic safely to the FTXUI main thread.
- **M2.4:** Support `Connection: keep-alive` persistent connections.

### Phase 3: HTTPS MITM (Weeks 4–6)
**Goal:** Intercept and decrypt HTTPS traffic.
- **M3.1:** Integrate `OpenSSL`.
- **M3.2:** Implement `CertGenerator` to dynamically generate a local Root CA and per-host leaf certificates.
- **M3.3:** Build thread-safe `CertCache`.
- **M3.4:** Implement `MitmSession` to handle the `CONNECT` method and ALPN HTTP/1.1 negotiation.

### Phase 4: Persistent Storage (Week 7)
**Goal:** Maintain session history across application restarts.
- **M4.1:** Integrate `SQLite3` and set up the schema and WAL journal mode.
- **M4.2:** Async writing of request/response data from proxy threads to disk.
- **M4.3:** Implement search/filtering directly on the DB for the History tab.

### Phase 5: Repeater (Week 8)
**Goal:** Allow users to edit and replay requests.
- **M5.1:** Add "Send to Repeater" hotkey.
- **M5.2:** Build editable multiline request component in the Repeater tab.
- **M5.3:** Wire the "Send" button to dispatch custom TCP/TLS raw requests and render the response.

### Phase 6: Extended Features (Weeks 9–11)
**Goal:** Advanced proxy capabilities.
- **M6.1:** **Intercept Mode:** Pause requests mid-flight, allowing edits before forwarding or dropping.
- **M6.2:** **Decoder Utility:** Add Base64, URL, and Hex encode/decode functionalities in the Decoder tab.

### Phase 7: Passive Scanner & Rules (Week 12)
**Goal:** Automate basic vulnerability hunting.
- **M7.1:** Parse response headers for missing security flags (e.g., `X-Frame-Options`).
- **M7.2:** Flag reflected user input in response bodies (potential XSS sinks).
- **M7.3:** Display issues in a new "Findings" or "Scanner" tab.

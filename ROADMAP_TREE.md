# BurpTUI Roadmap Tree

```text
BurpTUI Project Roadmap
├── Phase 1: MVP UI Shell (Week 1)
│   ├── Goal: Working FTXUI application with dummy data
│   ├── Setup CMake project & integrate FTXUI (vcpkg)
│   ├── Implement tab layout (Proxy, History, Repeater, Decoder)
│   ├── Build History tab dummy list and split-pane
│   └── Implement keyboard navigation
├── Phase 2: Plain HTTP Proxy (Weeks 2–3)
│   ├── Goal: Intercept and view real HTTP traffic
│   ├── Integrate Boost.Asio & llhttp
│   ├── Build ProxyServer & Session handler
│   ├── Implement thread-safe EventQueue for TUI updates
│   ├── Support keep-alive connections
│   └── Wire traffic to History tab
├── Phase 3: HTTPS MITM (Weeks 4–6)
│   ├── Goal: Intercept and decrypt HTTPS traffic
│   ├── Integrate OpenSSL
│   ├── Implement CertGenerator (Root CA & per-host certs)
│   ├── Build thread-safe CertCache
│   └── Implement MitmSession (CONNECT & ALPN HTTP/1.1)
├── Phase 4: Persistent Storage (Week 7)
│   ├── Goal: Maintain session history
│   ├── Integrate SQLite3 (WAL mode)
│   ├── Build SqliteStore for requests & responses
│   └── Add search/filtering to History tab
├── Phase 5: Repeater (Week 8)
│   ├── Goal: Edit and replay requests
│   ├── Add "Send to Repeater" hotkey
│   ├── Build multiline request editor
│   └── Implement custom request dispatch & response view
├── Phase 6: Extended Features (Weeks 9–11)
│   ├── Goal: Advanced proxy capabilities
│   ├── Intercept Mode (pause, edit, forward/drop)
│   └── Decoder Utility (Base64, URL, Hex, HTML entities)
└── Phase 7: Passive Scanner & Rules (Week 12+)
    ├── Goal: Automate basic vulnerability hunting
    ├── Parse response headers for missing security flags
    ├── Flag reflected user input (XSS sinks)
    ├── Display issues in Scanner/Findings tab
    ├── Intruder tool (payload iteration)
    └── WebSocket frame inspection
```

# BurpTUI Roadmap Tree

```text
BurpTUI Project Roadmap
│
├── Phase 1: MVP UI Shell (Week 1)                          → phases/phase1.md
│   ├── Goal: Working FTXUI application with dummy data
│   ├── Setup CMake project & integrate FTXUI (vcpkg)
│   ├── Implement tab layout (Proxy, History, Repeater, Decoder)
│   ├── Build History tab dummy list and split-pane
│   └── Implement keyboard navigation
│
├── Phase 2: Plain HTTP Proxy (Weeks 2–3)                   → phases/phase2.md
│   ├── Goal: Intercept and view real HTTP traffic
│   ├── Integrate Boost.Asio & llhttp
│   ├── Build ProxyServer & Session handler
│   ├── Implement thread-safe TsQueue for TUI updates
│   ├── Support keep-alive connections
│   └── Wire traffic to History tab
│
├── Phase 3: HTTPS MITM (Weeks 4–6)                        → phases/phase3.md
│   ├── Goal: Intercept and decrypt HTTPS traffic
│   ├── Integrate OpenSSL
│   ├── Implement CertGenerator (Root CA & per-host certs)
│   ├── Build thread-safe CertCache
│   └── Implement MitmSession (CONNECT & ALPN HTTP/1.1)
│
├── Phase 4: Persistent Storage (Week 7)                    → phases/phase4.md
│   ├── Goal: Maintain session history via SqliteStore
│   ├── Integrate SQLite3 (WAL mode)
│   ├── Build SqliteStore for HttpTransaction persistence
│   └── Add search/filtering to History tab
│
├── Phase 5: Repeater (Week 8)                              → phases/phase5.md
│   ├── Goal: Edit and replay requests
│   ├── Add "Send to Repeater" hotkey
│   ├── Build multiline request editor
│   └── Implement custom request dispatch & response view
│
├── Phase 6: Extended Features (Weeks 9–11)                 → phases/phase6.md
│   ├── Goal: Advanced proxy capabilities
│   ├── Intercept Mode (pause, edit, forward/drop)
│   └── Decoder Utility (Base64, URL, Hex, HTML entities)
│
└── Phase 7: Passive Scanner & Rules (Week 12+)             → phases/phase7.md
    ├── Goal: Automate basic vulnerability hunting
    ├── Parse response headers for missing security flags
    ├── Flag reflected user input (XSS sinks)
    ├── Display issues in Scanner/Findings tab
    ├── Intruder tool (payload iteration)
    └── WebSocket frame inspection
```

## Phase Dependency Chain

```text
Phase 1 (UI Shell)
    └──► Phase 2 (HTTP Proxy)
            └──► Phase 3 (HTTPS MITM)
                    └──► Phase 4 (Storage)
                            ├──► Phase 5 (Repeater)
                            │       └──► Phase 6 (Extended Features)
                            │               └──► Phase 7 (Scanner & Rules)
                            └──► Phase 6 can also start after Phase 4
```

## Features Not Yet Scheduled

The following features from `Plan.md` are not yet assigned to a specific phase:

- **Export requests as curl commands** — candidate for Phase 6 or Phase 7
- **Scope rules** (only intercept matching hosts) — candidate for Phase 7+
- **Match & Replace rules** (auto-modify requests by pattern) — candidate for Phase 7+

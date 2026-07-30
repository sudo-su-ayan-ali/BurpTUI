# BurpTUI Project Overview

## What is BurpTUI?
BurpTUI is a terminal‑based HTTP/HTTPS interception proxy written in modern C++17/20.  It mimics the core functionality of Burp Suite – intercepting traffic, letting you view, edit and replay requests – but runs entirely in the console using **FTXUI** for a rich TUI.

## Why use it?
- No GUI dependencies, works on any Linux terminal.
- Learn async networking with **Boost.Asio**, TLS with **OpenSSL**, and UI composition with **FTXUI**.
- Ideal for deep‑dives into HTTP/1.1, TLS hand‑shakes, and certificate generation.

---

## Key Features (phased)
| Phase | Feature | Description |
|------|----------|-------------|
| 1 | TUI Shell | Four tabs – Proxy, History, Repeater, Decoder – with dummy data. |
| 2 | Plain HTTP Proxy | Intercept real HTTP traffic, show requests in History. |
| 3 | HTTPS MITM | Dynamic per‑host cert generation, full TLS decryption. |
| 4 | Persistent Storage | SQLite DB stores every request/response, searchable history. |
| 5 | Repeater | Edit any captured request and resend, view diff. |
| 6+ | Extras | Intercept mode, decoder utilities, passive scanner, etc. |

---

## High‑Level Architecture
- **Main thread** runs the **FTXUI** `TuiApp`.
- **Boost.Asio** `io_context` runs on background threads handling network I/O.
- **EventQueue** (thread‑safe MPSC) pipes `TrafficEvent`s from proxy threads to the TUI via `ScreenInteractive::PostEvent`.
- **ProxyServer** accepts connections → creates **Session** (HTTP) or **MitmSession** (HTTPS).
- **HttpParser** (llhttp) parses raw traffic into `HttpRequest`/`HttpResponse` objects.
- **SqliteStore** persists these objects using the schema in `data/history.db`.
- **CertGenerator** / **CertCache** manage the root CA and per‑host certs for MITM.

---

## Repository Layout
```
BurpTUI/
├─ CMakeLists.txt               # top‑level build definition
├─ vcpkg.json & vcpkg-configuration.json
├─ README.md                    # full documentation (this repo)
├─ ca/                          # generated root CA (runtime, .gitignore)
├─ data/                        # runtime SQLite DB (gitignore)
├─ src/                         # source code
│   ├─ app/                     # App coordinator & config
│   ├─ proxy/                   # ProxyServer, Session, MitmSession, Cert* utilities
│   ├─ http/                    # HttpParser, request/response structs
│   ├─ storage/                 # Store abstractions, SQLite implementation
│   ├─ tui/                     # FTXUI components (tabs, widgets)
│   └─ util/                    # helpers (logger, encoding, thread‑safe queue)
└─ tests/                       # GoogleTest unit & integration tests
```

---

## Build & Run Quick Start
```bash
# Clone (already done) and install dependencies via vcpkg
vcpkg install
# Configure build (adjust VCPKG_ROOT if needed)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
# Build the binary
cmake --build build --parallel
# Run the proxy (default port 8080)
./build/burptui --port 8080
```
On first run the `ca/` directory and `ca.crt` are generated – import `ca.crt` into your browser's trusted store before intercepting HTTPS.

---

## Learning Path
1. **Modern C++** – smart pointers, move semantics, RAII.
2. **Boost.Asio** – `io_context`, async read/write, strands, optional C++20 coroutines.
3. **HTTP/1.1** – request/response format, chunked transfer, CONNECT method.
4. **TLS & X.509** – handshake flow, certificate generation with OpenSSL.
5. **FTXUI** – component model, event posting, layout composition.
6. **SQLite** – schema design, prepared statements, WAL mode.

---

## Where to Start
- Open `src/app/App.cpp` to see how the TUI is assembled.
- Check `src/proxy/ProxyServer.cpp` for the networking entry point.
- Browse `src/tui/HistoryTab.cpp` to understand how events are rendered.
- Run the unit tests in `tests/` for isolated component sanity.

---

## Known Limitations
- No HTTP/2 upstream support – forced to HTTP/1.1.
- Certificate‑pinned sites cannot be MITM‑ed.
- WebSocket inspection not implemented yet.
- Primarily Linux/macOS; Windows UI quirks may appear.

---

*Keep this file (`PROJECT_OVERVIEW.md`) as your first stop when you open the repo – it gives you a concise, yet complete, picture of the project without digging through every source file.*

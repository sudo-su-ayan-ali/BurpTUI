# BurpTUI — HTTP/HTTPS Interception Proxy in C++

A terminal-based HTTP/HTTPS interception proxy inspired by Burp Suite, built entirely in C++ with a rich TUI interface. Intercept, inspect, modify, and replay web traffic from your terminal.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Feature Scope](#feature-scope)
- [Tech Stack](#tech-stack)
- [What You Need to Learn](#what-you-need-to-learn)
- [Project Structure](#project-structure)
- [Architecture & Data Flow](#architecture--data-flow)
- [Build Phases](#build-phases)
- [Hard Problems & How to Solve Them](#hard-problems--how-to-solve-them)
- [Dependencies & Setup](#dependencies--setup)
- [Testing Strategy](#testing-strategy)
- [Known Limitations](#known-limitations)

---

## Project Overview

![alt text](<Pasted image.png>)

BurpTUI is a man-in-the-middle (MITM) HTTP/HTTPS proxy that runs in your terminal. It intercepts browser traffic, displays requests and responses in real time, and lets you modify and replay them — all without leaving the terminal.

**This is not a beginner project.** It requires solid C++17, understanding of async I/O, TLS internals, and HTTP protocol details. The plan below is designed to build knowledge incrementally so you are never blocked by gaps.

**Core loop:**
```
Browser → BurpTUI (port 8080) → Target Server
              ↕
         Terminal UI
    (view, edit, replay)
```

---

## Feature Scope

### Phase 1 — MVP (must have)

- HTTP/1.1 interception proxy on configurable port (default: 8080)
- HTTPS MITM via dynamic per-host TLS certificate generation
- Real-time request/response log in History tab
- Full request/response viewer with headers and body
- Repeater: copy any request, edit it, resend, diff response
- Decoder tab: Base64, URL encoding, HTML entities, hex

### Phase 2 — Extended (build after MVP is stable)

- Intercept mode: pause requests, allow edit before forwarding
- Search and filter on History (by host, method, status code, body content)
- SQLite persistent storage across sessions
- Passive scanner: flag missing security headers, open redirects, obvious XSS sinks
- Export requests as curl commands

### Phase 3 — Advanced (optional / later)

- Intruder: define payload positions, iterate payload list, log responses
- Scope rules: only intercept matching hosts
- Match & Replace rules: auto-modify requests by pattern
- WebSocket frame inspection

---

## Tech Stack

### Core Libraries

| Layer | Library | Why |
|---|---|---|
| TUI rendering | [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | Modern C++17, component-based, reactive. Gives tabs, inputs, scrollable lists, and split panels. |
| Async networking | [Boost.Asio](https://www.boost.org/doc/libs/release/libs/asio/) | Industry-standard async TCP/TLS I/O. Handles thousands of simultaneous connections without raw thread-per-connection. |
| TLS / HTTPS | [OpenSSL](https://www.openssl.org/) via Boost.Asio SSL | TLS termination, X.509 cert generation, ALPN negotiation. |
| HTTP parsing | [llhttp](https://github.com/nodejs/llhttp) | Node.js's HTTP parser, embeddable in C. Handles chunked encoding, pipelining, keep-alive. |
| Storage | [SQLite3](https://www.sqlite.org/) | Lightweight, embedded, no server needed. One file = full session history. |
| Build system | [CMake 3.20+](https://cmake.org/) | Standard. Use with vcpkg for dependency management. |
| Package manager | [vcpkg](https://vcpkg.io/) | Handles Boost, OpenSSL, FTXUI, SQLite, llhttp. |
| Testing | [GoogleTest](https://github.com/google/googletest) | Unit tests for parser, cert gen, storage, HTTP logic. |

### Standard Library Usage

- `std::thread`, `std::mutex`, `std::condition_variable` — proxy/TUI sync
- `std::unordered_map` — cert cache, header storage
- `std::optional`, `std::variant` — error handling without exceptions in hot paths
- `std::string_view` — zero-copy header parsing
- `std::shared_mutex` — concurrent read access to cert cache

### C++ Standard

**C++17 minimum.** C++20 coroutines (`co_await`) are recommended for Boost.Asio async code — they make the proxy session logic dramatically cleaner than callback chains.

---

## What You Need to Learn

Learn these in order. Each one unlocks the next phase of the build.

### 1. Modern C++17/20 (prerequisite — before anything else)

You cannot write safe async C++ without these:

- Smart pointers: `unique_ptr`, `shared_ptr`, `weak_ptr` and when to use each
- Move semantics: move constructor, `std::move`, perfect forwarding with `&&`
- Lambdas: capture by value vs reference, mutable lambdas, generic lambdas
- `std::optional`, `std::variant`, `std::string_view`
- RAII — resource cleanup via destructors, not manual `delete`
- Templates basics — you'll encounter them in Boost heavily

**Resources:** "A Tour of C++" (Stroustrup), cppreference.com, Jason Turner's C++ Weekly on YouTube.

### 2. Boost.Asio Async Model

This is the engine of the proxy. The concepts to master:

- `io_context` — the event loop
- `tcp::acceptor`, `tcp::socket` — listening and connecting
- `async_read`, `async_write`, `async_read_until` — non-blocking I/O
- `strand` — serialise handlers to avoid data races without locking
- C++20 coroutines with Asio (`co_await async_read(...)`) — much cleaner than callbacks

**Key mental model:** Asio is not threading. One thread running `io_context::run()` can handle thousands of connections via callbacks/coroutines. You only add threads for CPU-bound work.

**Practice task:** Write a TCP echo server in Asio that handles 100 simultaneous connections before touching the proxy code.

### 3. HTTP/1.1 Protocol Internals

You must know this from the byte level, not just conceptually:

- Request structure: request line, headers, blank line, optional body
- The `CONNECT` method — how browsers initiate HTTPS tunnels through proxies
- `Transfer-Encoding: chunked` — format of chunked bodies, why they exist
- `Content-Length` vs chunked — when each is used
- `Connection: keep-alive` and persistent connections
- `Host` header — required in HTTP/1.1, how you use it to route traffic

**Read:** RFC 7230 (HTTP/1.1 Message Syntax). All of it. It is short and dense, not long.

### 4. TLS and X.509 Certificate Internals

The most technically demanding knowledge area:

- TLS handshake sequence — ClientHello, ServerHello, Certificate, Finished
- What a certificate actually contains — subject, issuer, SANs, validity dates
- Certificate chains — root CA → intermediate → leaf cert
- ALPN (Application-Layer Protocol Negotiation) — how browser and server agree on HTTP/1.1 vs HTTP/2
- OpenSSL C API: `SSL_CTX`, `SSL_CTX_new`, `SSL_CTX_use_certificate`, `SSL_CTX_use_PrivateKey`
- X.509 generation: `X509_new`, `EVP_PKEY_new`, `RSA_generate_key_ex`, `X509_sign`

**MITM cert workflow:**
1. On startup: generate root CA (ca.key + ca.crt), save to disk
2. User installs ca.crt into browser trust store
3. Browser sends `CONNECT api.example.com:443`
4. Your proxy: generate a new cert for `api.example.com`, signed by your CA, with correct SAN
5. Browser validates the cert against your CA — succeeds
6. You now sit in the middle of the TLS connection

### 5. Multithreading and Synchronisation

- `std::thread`, `std::jthread` (C++20)
- `std::mutex` and `std::lock_guard` / `std::scoped_lock`
- `std::shared_mutex` for read-heavy caches (cert cache)
- `std::condition_variable` for blocking queues
- Lock-free queue via `std::atomic` for high-frequency event passing
- The critical rule: **never touch FTXUI components from a non-main thread**

### 6. FTXUI Component Model

- `ftxui::Component` vs `ftxui::Element` — the key distinction
- `Renderer`, `Input`, `Menu`, `Scroller`, `Container::Tab`, `Container::Vertical`
- `CatchEvent` — keyboard shortcuts, custom key handling
- `ScreenInteractive::PostEvent` — the only safe way to trigger a TUI refresh from a background thread
- Composing complex layouts with `hbox`, `vbox`, `flex`, `border`, `size`

### 7. SQLite C API

- `sqlite3_open` / `sqlite3_close`
- `sqlite3_prepare_v2` — compiled SQL statements (always use prepared statements, never string concat)
- `sqlite3_bind_text`, `sqlite3_bind_blob`, `sqlite3_bind_int64`
- `sqlite3_step`, `sqlite3_column_*`
- WAL mode (`PRAGMA journal_mode=WAL`) — allows concurrent reads while proxy is writing

---

## Project Structure

```
burptui/
├── CMakeLists.txt                  # top-level build definition
├── vcpkg.json                      # dependency manifest
├── vcpkg-configuration.json
├── .gitignore
├── README.md
│
├── ca/                             # generated at runtime, gitignored
│   ├── ca.crt                      # root CA cert — user installs this in browser
│   └── ca.key                      # root CA private key — never share this
│
├── data/                           # runtime data, gitignored
│   └── history.db                  # SQLite session history
│
├── src/
│   ├── main.cpp                    # entry point: parse args, wire components, start
│   │
│   ├── app/
│   │   ├── App.hpp                 # top-level coordinator, owns all subsystems
│   │   ├── App.cpp
│   │   └── Config.hpp              # port, CA paths, intercept mode, scope rules
│   │
│   ├── proxy/
│   │   ├── ProxyServer.hpp         # TCP acceptor on :8080, creates Sessions
│   │   ├── ProxyServer.cpp
│   │   ├── Session.hpp             # handles one browser↔proxy↔server connection
│   │   ├── Session.cpp             # plain HTTP flow
│   │   ├── MitmSession.hpp         # HTTPS CONNECT flow, TLS on both sides
│   │   ├── MitmSession.cpp
│   │   ├── CertGenerator.hpp       # generates per-host X.509 cert using OpenSSL
│   │   ├── CertGenerator.cpp
│   │   ├── CertCache.hpp           # thread-safe hostname → SSL_CTX cache
│   │   └── CertCache.cpp
│   │
│   ├── http/
│   │   ├── HttpParser.hpp          # wraps llhttp, emits HttpRequest/HttpResponse
│   │   ├── HttpParser.cpp
│   │   ├── HttpRequest.hpp         # struct: method, url, version, headers, body
│   │   ├── HttpRequest.cpp         # serialise back to raw bytes for forwarding
│   │   ├── HttpResponse.hpp        # struct: status_code, reason, headers, body
│   │   └── HttpResponse.cpp
│   │
│   ├── storage/
│   │   ├── Store.hpp               # abstract interface: save(), get(), list(), search()
│   │   ├── SqliteStore.hpp         # SQLite implementation
│   │   ├── SqliteStore.cpp
│   │   ├── MemoryStore.hpp         # in-memory fallback (no persistence)
│   │   └── MemoryStore.cpp
│   │
│   ├── tui/
│   │   ├── TuiApp.hpp              # FTXUI ScreenInteractive, tab router, event loop
│   │   ├── TuiApp.cpp
│   │   ├── ProxyTab.hpp            # intercept on/off toggle, live traffic counter
│   │   ├── ProxyTab.cpp
│   │   ├── HistoryTab.hpp          # scrollable request list, detail split-pane
│   │   ├── HistoryTab.cpp
│   │   ├── RepeaterTab.hpp         # request editor, send button, response viewer
│   │   ├── RepeaterTab.cpp
│   │   ├── DecoderTab.hpp          # encode/decode panel
│   │   ├── DecoderTab.cpp
│   │   └── Widgets.hpp             # shared FTXUI helpers: KeyValue, HexViewer, etc.
│   │
│   └── util/
│       ├── EventQueue.hpp          # thread-safe mpsc queue: proxy → TUI
│       ├── Logger.hpp              # file-based debug logger (not stdout — TUI owns that)
│       ├── Logger.cpp
│       ├── Encoding.hpp            # Base64, URL encode/decode, hex
│       ├── Encoding.cpp
│       └── StringUtils.hpp         # trim, split, case-insensitive compare, etc.
│
└── tests/
    ├── CMakeLists.txt
    ├── test_http_parser.cpp         # parse valid and malformed requests
    ├── test_cert_generator.cpp      # generate CA, sign leaf cert, verify chain
    ├── test_storage.cpp             # save/retrieve/search requests in SQLite
    ├── test_encoding.cpp            # Base64/URL encode-decode round trips
    └── test_event_queue.cpp         # concurrent push/pop correctness
```

---

## Architecture & Data Flow

### Component Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    Main Thread                          │
│                                                         │
│   ┌─────────────────────────────────────────────────┐  │
│   │              FTXUI TuiApp                       │  │
│   │   [Proxy Tab] [History Tab] [Repeater] [Decoder]│  │
│   └──────────────────────┬──────────────────────────┘  │
│                           │ PostEvent() on update       │
│              ┌────────────▼────────────┐                │
│              │      EventQueue         │                │
│              │  (thread-safe mpsc)     │                │
│              └────────────▲────────────┘                │
└───────────────────────────│─────────────────────────────┘
                            │ push events
┌───────────────────────────│─────────────────────────────┐
│               Asio io_context Thread(s)                 │
│                           │                             │
│   ┌───────────────────────┴──────────────────────────┐  │
│   │               ProxyServer                        │  │
│   │         tcp::acceptor on :8080                   │  │
│   └──────────┬────────────────────────┬──────────────┘  │
│              │ HTTP                   │ HTTPS CONNECT   │
│   ┌──────────▼──────────┐  ┌──────────▼──────────────┐  │
│   │      Session        │  │      MitmSession         │  │
│   │   plain HTTP flow   │  │   TLS both sides         │  │
│   └──────────┬──────────┘  └──────────┬───────────────┘  │
│              │                        │                  │
│   ┌──────────▼──────────────────────────────────────┐   │
│   │                  HttpParser                     │   │
│   │            llhttp-based request/response        │   │
│   └──────────┬──────────────────────────────────────┘   │
│              │                                           │
│   ┌──────────▼──────────┐   ┌──────────────────────┐    │
│   │     SqliteStore     │   │     CertCache         │    │
│   │  writes to disk     │   │  hostname→SSL_CTX     │    │
│   └─────────────────────┘   └──────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

### HTTPS MITM Sequence

```
Browser                 BurpTUI Proxy              Target Server
   │                         │                          │
   │── CONNECT host:443 ────►│                          │
   │◄─ 200 Connection Est. ──│                          │
   │                         │── TCP connect ──────────►│
   │                         │◄─ TCP accept ────────────│
   │── TLS ClientHello ─────►│                          │
   │   (expects host cert)   │── TLS ClientHello ──────►│
   │                         │◄─ TLS ServerHello ───────│
   │                         │   (real server cert)     │
   │◄─ TLS ServerHello ──────│                          │
   │   (fake cert, signed    │                          │
   │    by BurpTUI CA)       │                          │
   │── TLS Finished ────────►│                          │
   │◄─ TLS Finished ─────────│                          │
   │                    [two separate TLS sessions]     │
   │── HTTP GET /api ───────►│── HTTP GET /api ────────►│
   │                    [parse & log]                   │
   │◄─ HTTP 200 OK ──────────│◄─ HTTP 200 OK ───────────│
   │                    [parse & log]                   │
```

### Database Schema

```sql
CREATE TABLE requests (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   INTEGER NOT NULL,          -- unix ms
    method      TEXT    NOT NULL,          -- GET, POST, etc.
    host        TEXT    NOT NULL,
    path        TEXT    NOT NULL,
    http_version TEXT   NOT NULL,          -- HTTP/1.1
    req_headers TEXT    NOT NULL,          -- JSON object
    req_body    BLOB,
    status_code INTEGER,
    reason      TEXT,
    resp_headers TEXT,                     -- JSON object
    resp_body   BLOB,
    tls         INTEGER NOT NULL DEFAULT 0,  -- 0=HTTP, 1=HTTPS
    duration_ms INTEGER                    -- round-trip time
);

CREATE INDEX idx_host ON requests(host);
CREATE INDEX idx_timestamp ON requests(timestamp);
CREATE INDEX idx_status ON requests(status_code);
```

---

## Build Phases

### Phase 1 — TUI Shell (Week 1)

**Goal:** Working FTXUI application with four tabs and dummy data. No networking.

Tasks:
- Set up CMake project with FTXUI via FetchContent
- Implement tab layout: Proxy, History, Repeater, Decoder
- History tab: scrollable list of hardcoded `HttpRequest` structs
- Detail pane: click a request, see headers + body split vertically
- Keyboard navigation: Tab to switch tabs, arrow keys in list, `q` to quit

**Done when:** The TUI runs, you can navigate all tabs, and the layout looks correct.

**Do not move to Phase 2 until this is solid.** Layout bugs compound badly.

---

### Phase 2 — Plain HTTP Proxy (Weeks 2–3)

**Goal:** Intercept real HTTP traffic. No HTTPS yet.

Tasks:
- Add Boost.Asio to CMake
- Implement `ProxyServer` — TCP acceptor on port 8080
- Implement `Session` — async read from browser, parse with llhttp, forward to server, read response, forward back
- Implement `EventQueue` — thread-safe proxy-to-TUI event bus
- Wire new requests into History tab via `ScreenInteractive::PostEvent`
- Handle `Connection: keep-alive` — don't close the socket after every request

**Test with:** Configure Firefox to use `localhost:8080` as HTTP proxy. Browse to `http://neverssl.com`. Requests should appear in History.

**Done when:** 50 sequential HTTP requests all appear correctly in History without crash or hang.

---

### Phase 3 — HTTPS MITM (Weeks 4–6)

**Goal:** Intercept HTTPS traffic. This is the hardest phase.

Tasks:
- Add OpenSSL / Boost.Asio SSL to CMake
- Implement `CertGenerator` — generate root CA on first run, generate per-host certs signed by CA
- Implement `CertCache` — `shared_mutex`-protected hostname → `SSL_CTX` map
- Implement `MitmSession` — handle `CONNECT`, set ALPN to `http/1.1` only, TLS handshake with browser using fake cert, TLS handshake with server using real SNI
- Instruct user to install `ca.crt` into browser: Settings → Certificates → Import

**ALPN is critical:** Set `SSL_CTX_set_alpn_protos(ctx, (const unsigned char*)"\x08http/1.1", 9)` on the context you present to the browser. If you skip this, Chrome will negotiate HTTP/2 and you'll receive binary frames.

**Test with:** Browse to `https://httpbin.org/get`. Request should appear in History with `https://` prefix and full decrypted body.

**Done when:** 20 HTTPS requests to 10 different hosts all appear correctly. Certs are cached. No TLS handshake errors.

---

### Phase 4 — Storage + History (Week 7)

**Goal:** Persistent history across sessions, searchable.

Tasks:
- Add SQLite3 to CMake
- Implement `SqliteStore` using schema above
- Enable WAL mode on open (`PRAGMA journal_mode=WAL`)
- Write every request+response to DB after it completes
- Add search/filter bar to History tab: filter by host, method, status code
- Add detail view: selected request shows full headers and body in a scrollable pane

**Done when:** Close the proxy, reopen it, and all previous requests are still in History.

---

### Phase 5 — Repeater (Week 8)

**Goal:** Edit any request and resend it.

Tasks:
- Right-click (or keybind `r`) on any History entry to send to Repeater tab
- Repeater tab: multiline FTXUI `Input` editor for raw request (method, path, headers, body)
- "Send" button: open raw TCP or TLS socket to the original host, write the edited request, read response
- Response panel: side-by-side or stacked view of original vs new response
- Response diff highlighting (optional, ambitious)

**Done when:** You can take a POST request, change a parameter value, resend it, and see the different response.

---

### Phase 6 — Extras (Week 9+)

**Intercept mode:**
- Toggle intercept on/off (default off — too annoying during browsing)
- When on: pause each request, show in a queue, allow edit before forwarding
- Forward or Drop buttons

**Decoder tab:**
- Input box + format selector (Base64, URL, HTML entities, hex)
- Encode and Decode buttons
- Chain mode: output of one decode feeds input of next

**Passive scanner:**
- After response arrives, check: missing `X-Frame-Options`, `X-Content-Type-Options`, `Strict-Transport-Security`, `Content-Security-Policy`
- Flag reflected user input in response body (basic XSS sink detection)
- Show findings in a Findings tab

---

## Hard Problems & How to Solve Them

### 1. HTTP/2 Downgrade via ALPN

**Problem:** Chrome and Firefox negotiate HTTP/2 over TLS by default. HTTP/2 uses binary frames — your HTTP/1.1 parser will break immediately.

**Solution:** When building the `SSL_CTX` you present to the browser, set ALPN to advertise only `http/1.1`:
```cpp
static const unsigned char alpn[] = "\x08http/1.1";
SSL_CTX_set_alpn_protos(ctx, alpn, sizeof(alpn) - 1);
```
The browser sees you don't support HTTP/2 and falls back to HTTP/1.1. The upstream connection to the real server can still negotiate HTTP/2 separately — but for simplicity, also force HTTP/1.1 upstream.

### 2. Chunked Transfer Encoding

**Problem:** HTTP/1.1 responses frequently use `Transfer-Encoding: chunked`. The body arrives as `<hex-size>\r\n<data>\r\n` chunks. If you pass raw bytes to your body store, it will be corrupted.

**Solution:** llhttp's `on_body` callback fires with already-dechunked data — but only if you correctly set `llhttp_settings_t.on_body`. Do not try to parse the raw socket bytes manually. Let llhttp do it.

### 3. Thread Safety Between Proxy and TUI

**Problem:** Boost.Asio's io_context runs on a background thread. FTXUI's `ScreenInteractive::Loop()` runs on the main thread. Modifying any FTXUI `Component` from the Asio thread causes data races and crashes.

**Solution:**
```cpp
// In EventQueue.hpp — a simple thread-safe queue
struct TrafficEvent { HttpRequest req; HttpResponse resp; };

class EventQueue {
    std::queue<TrafficEvent> queue_;
    std::mutex mutex_;
public:
    void push(TrafficEvent e) {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(e));
        screen_.PostEvent(ftxui::Event::Custom); // wake the TUI loop
    }
    std::optional<TrafficEvent> pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        auto e = std::move(queue_.front());
        queue_.pop();
        return e;
    }
};
```
The TUI's `CatchEvent` handler calls `queue.pop()` on every `Event::Custom` and updates its local state. No FTXUI component is ever touched from the Asio thread.

### 4. TLS Certificate Caching

**Problem:** Generating a new RSA-2048 key and X.509 cert per HTTPS connection takes 50–200ms and hammers the CPU.

**Solution:** Cache `SSL_CTX*` objects keyed by hostname. Use `std::shared_mutex` — multiple concurrent reads (common on cache hit) don't block each other:
```cpp
class CertCache {
    std::unordered_map<std::string, std::shared_ptr<SSL_CTX_Wrapper>> cache_;
    mutable std::shared_mutex mutex_;
public:
    std::shared_ptr<SSL_CTX_Wrapper> get(const std::string& host);
    void put(const std::string& host, std::shared_ptr<SSL_CTX_Wrapper> ctx);
};
```

### 5. Large Response Bodies

**Problem:** Buffering a 100MB video response in memory will OOM the proxy and block the browser.

**Solution:** Stream bytes from server to browser as they arrive. Only accumulate up to a configurable limit (e.g. 1MB) in the body field stored to DB. For larger bodies, store the first 1MB and flag the rest as truncated. Display body size in History for truncated entries.

### 6. Logging to stdout Breaks TUI

**Problem:** Any `std::cout` or `printf` from the proxy thread will corrupt the FTXUI rendering — it draws over the TUI output.

**Solution:** Never use stdout after the TUI starts. Use a file-based logger:
```cpp
// In util/Logger.hpp
Logger::get().log("[proxy] new connection from " + remote_addr);
// writes to debug.log, never stdout
```

### 7. Certificate Pinning Sites

**Problem:** Some sites (browser update endpoints, certain APIs) use certificate pinning — they reject any cert not matching their hardcoded public key fingerprint, including your CA-signed fake cert.

**Solution:** This is expected behaviour, not a bug. Burp Suite itself cannot MITM pinned connections. Add a bypass list: connections to pinned hosts are passed through as opaque TCP tunnels without interception. The proxy logs them as `[PINNED — not intercepted]`.

---

## Dependencies & Setup

### vcpkg.json

```json
{
  "name": "burptui",
  "version": "0.1.0",
  "dependencies": [
    "boost-asio",
    "boost-system",
    "openssl",
    "ftxui",
    "sqlite3",
    "llhttp",
    "gtest"
  ]
}
```

### CMakeLists.txt (top-level sketch)

```cmake
cmake_minimum_required(VERSION 3.20)
project(burptui CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(burptui
    src/main.cpp
    src/app/App.cpp
    src/proxy/ProxyServer.cpp
    src/proxy/Session.cpp
    src/proxy/MitmSession.cpp
    src/proxy/CertGenerator.cpp
    src/proxy/CertCache.cpp
    src/http/HttpParser.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/storage/SqliteStore.cpp
    src/storage/MemoryStore.cpp
    src/tui/TuiApp.cpp
    src/tui/ProxyTab.cpp
    src/tui/HistoryTab.cpp
    src/tui/RepeaterTab.cpp
    src/tui/DecoderTab.cpp
    src/util/Logger.cpp
    src/util/Encoding.cpp
)

target_link_libraries(burptui PRIVATE
    Boost::system
    OpenSSL::SSL OpenSSL::Crypto
    ftxui::screen ftxui::dom ftxui::component
    SQLite::SQLite3
)

enable_testing()
add_subdirectory(tests)
```

### First Run

```bash
# Clone and set up
git clone <repo>
cd burptui
vcpkg install

# Build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --parallel

# Run
./build/burptui --port 8080

# On first run:
# - ca/ directory is created
# - ca/ca.crt is generated
# - Install ca.crt into your browser's certificate store
# - Set browser proxy to localhost:8080
```

---

## Testing Strategy

### Unit Tests

| File | Tests |
|---|---|
| `test_http_parser.cpp` | Parse valid GET, POST, chunked response, malformed request, missing headers |
| `test_cert_generator.cpp` | Generate CA, generate leaf cert, verify leaf is signed by CA, check SAN |
| `test_storage.cpp` | Insert request, retrieve by id, list all, search by host, search by method |
| `test_encoding.cpp` | Base64 encode/decode round trips, URL encode/decode, edge cases (empty, binary) |
| `test_event_queue.cpp` | Concurrent push from 10 threads, single consumer, verify no events lost |

### Integration Tests

Run the proxy binary and use curl as the browser:

```bash
# HTTP
curl -x http://localhost:8080 http://httpbin.org/get
# Should appear in History

# HTTPS (after installing ca.crt)
curl -x http://localhost:8080 --cacert ca/ca.crt https://httpbin.org/get
# Should appear in History with decrypted body
```

### Manual Testing Checklist

- [ ] 50 HTTP requests: all appear in History, no duplicates, no missing bodies
- [ ] 50 HTTPS requests across 20 hosts: all decrypted, certs cached
- [ ] Repeater: modify and resend 10 different requests, responses correct
- [ ] History search: filter by host returns only matching entries
- [ ] Restart proxy: previous requests still in History (SQLite persistence)
- [ ] Large response (>1MB): proxy does not OOM, body truncated in display
- [ ] Keep-alive: 10 requests on one connection all captured
- [ ] Chunked response: body reassembled correctly, chunk markers not stored

---

## Known Limitations

These are permanent constraints, not bugs to fix:

- **HTTP/2 to upstream not supported.** All upstream connections are forced to HTTP/1.1. Sites that require HTTP/2 (rare) may behave differently than in a direct browser connection.
- **Certificate-pinned sites cannot be intercepted.** This is true of all MITM proxies including Burp Suite. Pinned connections are passed through unmodified.
- **WebSockets are passed through in Phase 1–5.** Full WebSocket frame inspection is a Phase 3 extra feature.
- **No mobile device proxy support** without additional network routing setup. This tool runs as a local proxy only.
- **Windows support is possible** but not the primary target. Boost.Asio and OpenSSL are cross-platform, but FTXUI terminal behaviour differs on Windows. Linux and macOS are the primary targets.

---

## Milestone Summary

| Milestone | Deliverable | Target |
|---|---|---|
| M1 | TUI shell, 4 tabs, keyboard nav | Week 1 |
| M2 | HTTP proxy, real requests in History | Week 3 |
| M3 | HTTPS MITM working | Week 6 |
| M4 | SQLite storage, search | Week 7 |
| M5 | Repeater working | Week 8 |
| M6 | Intercept mode + Decoder | Week 10 |
| M7 | Passive scanner | Week 12 |
# High-Level Design (HLD) & Technical Architecture: BurpTUI

## 1. Introduction
This document defines the system architecture, component boundaries, thread/concurrency models, and data flows for the BurpTUI project.

## 2. System Architecture & Component Boundaries
BurpTUI is built using a component-based architecture separated into distinct layers to isolate the User Interface (TUI) from the Network and Storage engines.

### 2.1 Core Components
- **App Coordinator (`src/app/`)**: Manages the application lifecycle, wireframes subsystems, and handles configuration (port, CA paths).
- **Proxy Engine (`src/proxy/`)**: Uses `Boost.Asio` to handle TCP/TLS connections. Responsible for HTTP and HTTPS (MITM) session states.
- **HTTP Parser (`src/http/`)**: Wraps `llhttp` for zero-copy parsing of raw bytes into structured `HttpRequest` and `HttpResponse` objects.
- **Crypto Engine (`src/proxy/Cert*`)**: Uses `OpenSSL` to dynamically generate X.509 certificates and manages the thread-safe `CertCache`.
- **Storage Layer (`src/storage/`)**: Uses `SQLite3` to persist traffic data.
- **TUI Layer (`src/tui/`)**: Uses `FTXUI` to render the interface, process user inputs, and display traffic.

## 3. Data Flow Diagram
Below is the data flow representing how traffic moves from the browser, through the proxy, to the target server, and how events are piped to the TUI.

```mermaid
flowchart TD
    Browser[Browser / Client] <-->|TCP / TLS| ProxyServer[ProxyServer :8080]
    ProxyServer --> Session[Session / MitmSession]
    Session <-->|TCP / TLS| Target[Target Server]
    
    Session --> HttpParser[HttpParser (llhttp)]
    HttpParser --> SqliteStore[(SQLite DB)]
    HttpParser --> EventQueue[EventQueue (MPSC)]
    
    EventQueue -->|Push TrafficEvent| TUI[FTXUI Main Thread]
    
    subgraph Background Threads
    ProxyServer
    Session
    HttpParser
    SqliteStore
    end
    
    subgraph Main Thread
    TUI
    end
```

## 4. Threading and Concurrency Model
To prevent blocking the terminal UI, the application strictly separates CPU-bound/Network operations from the UI loop.

- **Main Thread (UI):** Exclusively runs the `ftxui::ScreenInteractive::Loop()`. **Critical Rule:** No FTXUI components can be modified from outside this thread.
- **Boost.Asio `io_context` Thread(s):** Background thread(s) handling async read/writes via non-blocking I/O.
- **Inter-Thread Communication:** Achieved via an `EventQueue` (a thread-safe Multi-Producer Single-Consumer queue). When the Asio thread pushes a `TrafficEvent`, it calls `ScreenInteractive::PostEvent(ftxui::Event::Custom)` to safely wake the UI thread to pull the event.
- **Certificate Caching:** A `std::shared_mutex` protects the OpenSSL `SSL_CTX` cache, allowing concurrent read operations (cache hits) without blocking.

## 5. Performance Targets
- **Throughput:** Handle 100 simultaneous concurrent connections.
- **Latency:** Interception overhead should not exceed 10ms for HTTP and 50ms for HTTPS (excluding initial cert generation).
- **Memory Footprint:** Keep active memory under 150MB by streaming large response bodies (buffering up to a 1MB limit for history display, truncating the rest).
- **Disk I/O:** Utilize SQLite WAL (`Write-Ahead Logging`) mode to allow the proxy to write data asynchronously while the TUI reads from the database without database locking issues.

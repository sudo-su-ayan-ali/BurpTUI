# Functional Specification Document (FSD): BurpTUI

## 1. Introduction
This Functional Specification Document (FSD) outlines the exact behavior, user workflows, and system responses for BurpTUI, directly derived from the PRD.

## 2. System Architecture & Components
- **UI Thread:** The main thread running the FTXUI loop, responsible for rendering and user inputs.
- **Network Threads:** Background threads running a `Boost.Asio` `io_context` to handle non-blocking network I/O.
- **Event Queue:** A thread-safe Multi-Producer Single-Consumer (MPSC) queue pushing `TrafficEvent`s from the proxy to the UI.
- **Storage Layer:** SQLite database wrapping queries via prepared statements, ensuring persistent traffic storage.
- **Crypto Engine:** OpenSSL-based certificate generator for Man-in-the-Middle (MITM) attacks.

## 3. Detailed Feature Specifications

### 3.1 Initial Setup & CA Generation
**Workflow:**
1. User executes `./burptui --port 8080`.
2. System checks for the existence of `ca/ca.crt` and `ca/ca.key`.
3. If missing, system generates a new Root CA using OpenSSL and saves it to the `ca/` directory.
4. TUI is initialized and displayed on the terminal.
**Error Logic:**
- If the directory `ca/` cannot be created (permissions issue), gracefully exit with a clear terminal error.
- If port `8080` is already in use, display a fatal error message in the terminal and exit.

### 3.2 TUI Shell & Navigation
**Workflow:**
1. The screen renders four primary tabs: `Proxy`, `History`, `Repeater`, `Decoder`.
2. Users can navigate between tabs using keyboard arrows (`Left`/`Right`) or `Tab`.
**Functional Behavior:**
- The interface must remain responsive.
- UI elements update asynchronously when `TrafficEvent`s are posted via `ScreenInteractive::PostEvent`.

### 3.3 HTTP Proxy Interception
**Workflow:**
1. Client (e.g., browser) connects to the proxy port.
2. The proxy accepts the connection and creates a `Session`.
3. `llhttp` parses the incoming HTTP stream into an `HttpRequest` object.
4. Proxy forwards the request to the upstream server, retrieves the response, and sends it back to the client.
5. The transaction is packaged into a `TrafficEvent` and pushed to the UI and SQLite.

### 3.4 HTTPS MITM (Man-in-the-Middle)
**Workflow:**
1. Client initiates an HTTP `CONNECT` request for a target host (e.g., `example.com:443`).
2. Proxy responds with `200 Connection Established`.
3. System checks the `CertCache` for a valid certificate for `example.com`.
4. If missing, `CertGenerator` dynamically creates an X.509 certificate for `example.com` signed by the local Root CA.
5. Proxy establishes a TLS connection with the client using the generated cert, and a separate TLS connection to the upstream server.
6. Decrypted traffic is parsed identically to plain HTTP.
**Error Logic:**
- If the upstream server rejects the TLS handshake or the certificate cannot be generated, the proxy drops the client connection gracefully.
- Certificate-pinned applications will inherently fail the client-side handshake; this is expected behavior.

### 3.5 Persistent Storage (SQLite)
**Functional Behavior:**
- Database file is stored at `data/history.db`.
- Operates in WAL (Write-Ahead Logging) mode to prevent locks between read (UI) and write (Proxy) threads.
- All requests/responses are stored asynchronously.
- The History tab queries the DB for display, paginating or limiting results to prevent memory bloat.
**Error Logic:**
- If the DB file is corrupted, the system should prompt to delete or repair it.

### 3.6 Repeater
**Workflow:**
1. User highlights a request in the History tab and presses a designated key (e.g., `r` or `Ctrl+R`) to send it to the Repeater tab.
2. In the Repeater tab, an editable text area allows modifying headers, URI, or body.
3. User presses `Send`.
4. System constructs a raw HTTP request, establishes a new connection (TCP or TLS) to the target host, and sends the data.
5. The response is parsed and rendered in the adjacent Repeater response panel.
**Input Validations:**
- Ensure the `Host` header aligns with the intended target.
- Reject completely malformed HTTP requests prior to sending, or gracefully handle the resulting parsing error.

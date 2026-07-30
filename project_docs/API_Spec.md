# API & Internal Interfaces Specification: BurpTUI

## 1. Overview
As an interception proxy, BurpTUI operates around two types of interfaces:
1. **Internal API / Event Bus:** The structural components (`HttpRequest`, `HttpResponse`) and the event system used to communicate between the networking backend and the UI frontend.
2. **External HTTP Protocol Parsing:** The standards and exact HTTP semantics the proxy expects and handles.

## 2. Internal Event API (Proxy to TUI)

### 2.1 Request & Response Data Structures
The parser layer standardizes all HTTP traffic into C++ structs before storage or UI rendering.

```cpp
struct HttpRequest {
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
};

struct HttpResponse {
    int status_code;
    std::string reason;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
};
```

### 2.2 EventQueue & TrafficEvent
The proxy communicates with the UI strictly through a thread-safe Multi-Producer Single-Consumer (MPSC) queue.

**Event Structure:**
```cpp
struct TrafficEvent {
    HttpRequest req;
    HttpResponse resp;
    int id; // DB insertion ID
    bool is_tls;
};
```

**Interface Requirements:**
- The background thread calls `EventQueue::push(TrafficEvent)`.
- The `push()` method must subsequently call `ScreenInteractive::PostEvent(ftxui::Event::Custom)` to safely notify the FTXUI main thread.
- The UI thread consumes the queue via `EventQueue::pop()`.

## 3. HTTP Protocol Handling Specification

### 3.1 Plain HTTP Routing
- The proxy intercepts standard HTTP requests.
- The destination is determined dynamically by reading the `Host` header (HTTP/1.1 requirement).

### 3.2 HTTPS CONNECT Method Handling
When a browser intends to establish a TLS connection, it sends an HTTP `CONNECT` request.
- **Request Format:** `CONNECT target.com:443 HTTP/1.1`
- **Proxy Response:** Must return `HTTP/1.1 200 Connection Established\r\n\r\n`.
- **Action:** Post-200 response, the proxy hands the socket over to the `MitmSession` handler, which performs an OpenSSL TLS handshake presenting a dynamically generated X.509 certificate for `target.com`.

### 3.3 ALPN Protocol Negotiation
Browsers default to negotiating HTTP/2 over TLS. The llhttp parser only supports HTTP/1.1.
- **Specification:** When building the `SSL_CTX` presented to the browser, the ALPN (Application-Layer Protocol Negotiation) must strictly advertise `http/1.1`.
- **Implementation Required:** `SSL_CTX_set_alpn_protos(ctx, (const unsigned char*)"\x08http/1.1", 9)`

### 3.4 Chunked Transfer Encoding
- The proxy must accurately handle `Transfer-Encoding: chunked`.
- Raw chunk markers (`<hex-size>\r\n`) must be stripped by `llhttp` before storing the `resp_body` into the DB or pushing to the UI.

### 3.5 Certificate Pinning & Passthrough
- Applications implementing Certificate Pinning will reject BurpTUI's dynamically generated certificates.
- The proxy must detect TLS handshake failures gracefully, terminating the client connection without crashing the background thread.

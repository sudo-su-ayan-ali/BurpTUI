# Phase 2: Plain HTTP Proxy (Weeks 2–3)

## Goal
The primary objective of this phase is to intercept and view real HTTP traffic by building a functional proxy server backend. This involves managing network connections, parsing HTTP protocols, establishing thread-safe communication between the networking backend and the TUI frontend, and displaying the captured traffic live in the History tab.

---

## 1. Integrate Boost.Asio & llhttp

To handle high-performance networking and accurate HTTP parsing, we will introduce two robust libraries to the technology stack.

### Step 1.1: Dependency Management Update
- Add `boost-asio` and `llhttp` as dependencies in your package manifest (e.g., `vcpkg.json`).
- Ensure the CMake configuration is updated to find these packages and link them properly to your main executable.
- Validate that the build system can fetch and compile these new dependencies without breaking the existing FTXUI setup.

### Step 1.2: Architecture and Threading Strategy
- Establish a clear separation of concerns between the UI thread and the networking thread.
- Design the application lifecycle so that the FTXUI event loop runs on the main thread, while Boost.Asio's asynchronous event loop (`io_context`) runs on a dedicated background thread pool.
- Prepare the project structure to house networking components in separate modules to keep the codebase organized.

---

## 2. Build ProxyServer & Session Handler

The core of the proxy is a listener that accepts incoming connections and a session manager that brokers communication between the client (browser) and the target server.

### Step 2.1: Implement the Listener (ProxyServer)
- Create a `ProxyServer` class responsible for initializing the Boost.Asio `io_context` and binding a TCP acceptor to a designated local port (e.g., 8080).
- Implement an asynchronous accept loop that continuously listens for incoming client connections.
- Upon accepting a new connection, the server should dynamically allocate a new `Session` object and transfer the client socket ownership to it.

### Step 2.2: Implement the Session Handler
- Create a `Session` class to handle the lifecycle of an individual TCP connection.
- Establish a dual-socket architecture within the session: one socket connected to the client, and a second socket that will dynamically connect to the upstream server based on the client's HTTP request.
- Implement asynchronous read operations to buffer incoming data from the client.

### Step 2.3: HTTP Parsing with llhttp
- Integrate `llhttp` parsers into the session's read pipeline. You will need separate parser instances for the client-to-proxy flow (parsing requests) and the proxy-to-client flow (parsing responses).
- Configure parser callbacks (e.g., on URL, on header field, on header value, on message complete) to extract critical routing information such as the target `Host` header, HTTP method, and path.
- Use the extracted `Host` header to resolve the target server's IP address asynchronously and establish the upstream TCP connection.
- Once connected, implement an asynchronous read/write loop that forwards raw bytes between the client and the upstream server, simultaneously feeding the traffic through the HTTP parsers for logging.

---

## 3. Implement Thread-Safe EventQueue for TUI Updates

Since the networking backend and the TUI frontend run on different threads, you need a mechanism to safely pass captured traffic data to the UI without race conditions, UI lockups, or memory corruption.

### Step 3.1: Design the Event Queue Structure
- Create a thread-safe message queue utilizing standard library concurrency primitives (e.g., `std::mutex` and `std::lock_guard` around a standard queue container).
- Define a strict data payload structure (a "Transaction Event") containing the parsed HTTP request and response details (method, path, headers, body, status code) cleanly formatted for UI consumption.

### Step 3.2: Backend Publisher Implementation
- Inside the `Session` handler, once a full HTTP request/response cycle is successfully parsed and logged, construct an event object containing copies of the relevant data.
- Safely acquire the lock and push this event object into the shared thread-safe queue.

### Step 3.3: Frontend Consumer Implementation
- FTXUI provides a thread-safe mechanism to post UI update events (custom events that wake up the main loop). Leverage this by having the networking thread post a custom "Data Ready" trigger to the FTXUI screen.
- Upon receiving this trigger, prompt the UI thread to lock the queue, pop all newly available items, and append them to its local state.

---

## 4. Support Keep-Alive Connections

Modern HTTP relies heavily on keep-alive connections to reuse TCP sockets for multiple requests, drastically reducing connection latency.

### Step 4.1: Connection State Tracking
- Enhance the `Session` class to track the connection state based on HTTP headers (e.g., evaluating `Connection: keep-alive` versus `Connection: close`) and the HTTP version (HTTP/1.1 defaults to keep-alive).
- Manage the `llhttp` parser states carefully so they are properly reset between consecutive requests flowing over the exact same socket.

### Step 4.2: Lifecycle Management
- Modify the data forwarding loop to remain active after a response completes, looping back to read the next request from the client socket.
- Implement robust timeout mechanisms using Boost.Asio timers. If a keep-alive connection remains idle beyond a specific threshold, safely close both client and upstream sockets and cleanly destroy the session to prevent resource leaks.
- Ensure graceful error handling so that if the upstream server prematurely closes the connection, the client is informed and the session state is torn down correctly.

---

## 5. Wire Traffic to History Tab

The final step connects the live backend logic to the FTXUI frontend you built in Phase 1.

### Step 5.1: State Synchronization
- Replace the static synthetic data in your TUI state with a dynamic collection (e.g., a `std::vector` of transaction objects).
- When the UI thread pulls new traffic events from the thread-safe queue, append these objects to the dynamic collection in memory.

### Step 5.2: Dynamic UI Refresh
- Ensure that appending new items to the data collection automatically triggers a re-render of the History tab's list menu.
- Update the detail pane renderer to fetch data from the newly populated structures, ensuring that selecting a live item accurately displays its specific headers and body content.
- Implement dynamic list indexing so that as new requests stream in, the menu boundaries expand correctly, allowing the user to seamlessly scroll through real-time traffic.

By completing these steps, the application will transform from an empty visual shell into a functional proxy capable of intercepting, parsing, and visualizing live, unencrypted HTTP traffic.

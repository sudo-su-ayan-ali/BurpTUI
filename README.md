# BurpTUI: Terminal-Based Interception Proxy

## 1. Project Introduction
BurpTUI is a lightweight, blazing-fast, terminal-based HTTP/HTTPS interception proxy inspired by Burp Suite. Built entirely in modern C++ (C++17/20), it allows security professionals, developers, and students to intercept, inspect, modify, and replay web traffic directly from the terminal without relying on a bulky GUI.

## 2. Architecture Summary
BurpTUI utilizes a highly concurrent, component-based architecture:
- **TUI Layer (FTXUI):** A reactive terminal interface running on the main thread.
- **Network Engine (Boost.Asio):** Handles thousands of asynchronous TCP and TLS connections on background threads.
- **Crypto Engine (OpenSSL):** Dynamically generates per-host X.509 certificates to Man-in-the-Middle (MITM) HTTPS traffic.
- **HTTP Parser (llhttp):** Zero-copy, high-performance parsing of raw HTTP streams.
- **Storage Layer (SQLite3):** Persists traffic history locally using Write-Ahead Logging (WAL) for lock-free concurrent access.
- **Event Bus:** A thread-safe MPSC queue securely pipes network events to the UI thread.

## 3. Prerequisite Software
Ensure your development environment meets the following requirements before building:
- **Compiler:** GCC 10+, Clang 11+, or MSVC 19.29+ (Must support C++20).
- **Build System:** CMake (v3.20 or newer).
- **Package Manager:** `vcpkg` (Used to fetch Boost, OpenSSL, FTXUI, SQLite, and llhttp).

## 4. Build Commands
BurpTUI uses CMake combined with `vcpkg` for seamless dependency management.

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/BurpTUI.git
cd BurpTUI

# 2. Install dependencies via vcpkg
vcpkg install

# 3. Configure the CMake project (Adjust VCPKG_ROOT to your local vcpkg installation)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 4. Compile the project
cmake --build build --parallel
```

## 5. Quick-Start Instructions

### Running the Proxy
Start the proxy on the default port (8080):
```bash
./build/burptui --port 8080
```

### HTTPS Interception Setup (First Run)
1. On the first launch, BurpTUI will automatically generate a custom Root CA certificate and private key in the `ca/` directory.
2. Locate the generated certificate: `ca/ca.crt`.
3. Open your web browser's Certificate Manager and **Import** `ca.crt` into the "Trusted Root Certification Authorities" store.
4. Configure your browser or OS to use `localhost:8080` as its HTTP and HTTPS proxy.
5. Traffic will now populate in the BurpTUI **History** tab!

*(Note: To uninstall, simply delete the `ca/` directory and remove the certificate from your browser's trust store.)*

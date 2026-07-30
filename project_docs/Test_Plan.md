# Test Plan & Quality Assurance Strategy: BurpTUI

## 1. Quality Goals
Given the complex nature of async memory management and networking, testing in BurpTUI focuses on absolute stability. A crash in the proxy stops all browser traffic.

## 2. Unit Testing Strategy
Uses **GoogleTest** (`gtest`). Unit tests validate subsystems in isolation without launching the FTXUI interface.

| Component | Target Coverage Areas |
|---|---|
| **HTTP Parser** | Valid GET/POST, chunked encoding reassembly, malformed requests, missing headers. |
| **Cert Generator** | Root CA generation, leaf cert generation, verification that the leaf is properly signed by the CA, SAN validation. |
| **Storage Engine** | Insert/retrieve via SQLite, search by host/method, handling of 1MB+ blob truncations. |
| **Encodings** | Base64/URL encode & decode round trips, including edge cases (empty strings, binary data). |
| **Event Queue** | Thread-safety check: Concurrent push from 10 threads and single pop to verify no events are lost or mangled. |

## 3. Integration Testing
Automated testing of the network layer using `curl` against a running instance of the proxy binary.

- **HTTP Check:** `curl -x http://localhost:8080 http://httpbin.org/get`
- **HTTPS Check:** `curl -x http://localhost:8080 --cacert ca/ca.crt https://httpbin.org/get`
- **Validation:** Both requests must successfully complete and log their exact payloads into the SQLite database.

## 4. Manual QA & Edge Case Checklist
Before any major release, the following scenarios must be validated manually:
- [ ] Large response bodies (>10MB) do not cause OOM and are properly truncated in the UI.
- [ ] Certificate Pinned sites (e.g., browser update endpoints) are bypassed seamlessly without crashing the proxy.
- [ ] Editing a payload in Repeater and sending a malformed HTTP request doesn't crash the proxy loop.
- [ ] The TUI remains entirely responsive during heavy traffic floods (e.g., navigating a complex, media-heavy website).

## 5. CI/CD Pipeline Configuration
A GitHub Actions pipeline is required for all Pull Requests:
1. **Build Step:** Matrix build across Linux (Ubuntu) and macOS using CMake + vcpkg.
2. **Unit Tests:** Execute `ctest` to run the GoogleTest suite.
3. **Integration Tests:** Spin up a background BurpTUI process, run `curl` test scripts against it, and assert success codes.
4. **Memory Check (Optional but recommended):** Run unit tests through `Valgrind` or ASAN to catch memory leaks in C++ async handlers.

# Product Requirements Document (PRD): BurpTUI

## 1. Product Vision
BurpTUI aims to be a lightweight, fast, and terminal-based HTTP/HTTPS interception proxy. By mimicking the core functionalities of the popular Burp Suite (intercepting traffic, viewing history, editing and replaying requests) without relying on a graphical user interface, BurpTUI provides an accessible and performant alternative that runs entirely within the console. It also serves as an educational foundation for learning modern C++, async networking, TLS, and TUI composition.

## 2. Product Goals
- **Accessibility:** Ensure the application can run on any standard Linux/macOS terminal without GUI dependencies (like X11 or Wayland).
- **Core Proxy Functionality:** Successfully intercept plain HTTP and dynamically decrypt HTTPS traffic (MITM).
- **Usability:** Deliver a rich terminal user interface (TUI) utilizing FTXUI with intuitive tabs (Proxy, History, Repeater, Decoder).
- **Educational Value:** Build the tool using modern C++17/20, Boost.Asio, OpenSSL, and SQLite, providing clear examples of asynchronous operations, threading, and data persistence.

## 3. Target Audience
- **Security Professionals & Pentesters:** Individuals needing a quick, CLI-native tool for API testing and web vulnerability hunting over SSH or in constrained environments.
- **Backend Developers:** Engineers debugging HTTP traffic, API endpoints, or Webhooks who prefer staying in the terminal.
- **Students & Learners:** Developers looking to understand the inner workings of HTTP/1.1, TLS handshakes, dynamic certificate generation, and async network programming in C++.

## 4. User Personas
### Persona 1: Alice, the AppSec Engineer
- **Needs:** A fast way to intercept and replay HTTP requests on a remote server via SSH.
- **Pain Points:** Burp Suite requires a GUI, which is slow and cumbersome over X11 forwarding or when working purely on headless servers.

### Persona 2: Bob, the C++ Student
- **Needs:** An open-source project to study modern C++ architecture, specifically asynchronous socket I/O and OpenSSL integration.
- **Pain Points:** Existing proxies are either too simplistic, written in memory-unsafe languages, or too complex to easily dissect.

## 5. Core Features
1. **TUI Shell:** A rich, responsive console interface featuring four primary tabs: Proxy, History, Repeater, and Decoder.
2. **Plain HTTP Proxy:** Core capability to intercept and log unencrypted HTTP traffic.
3. **HTTPS MITM:** On-the-fly decryption of TLS traffic via dynamic per-host certificate generation using a custom Root CA.
4. **Persistent Storage:** An embedded SQLite database for persisting captured request/response histories and facilitating search.
5. **Repeater:** Functionality to take a captured request, edit its parameters or headers, and resend it to observe the server's response.

## 6. Success Metrics (KPIs)
- **Functional:** 100% success rate in dynamically generating certificates and intercepting HTTPS traffic for non-pinned sites.
- **Performance:** Handle at least 100 concurrent connections without significant TUI lag or dropped frames.
- **Stability:** Zero crashes during normal usage, with proper thread synchronization between the TUI and background Boost.Asio tasks.
- **Usability:** Users can successfully setup the proxy, install the root CA, and replay a modified request in under 5 minutes.

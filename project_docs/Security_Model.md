# Security & Threat Model: BurpTUI

## 1. Overview
BurpTUI is a security tool intended to intercept, decrypt, and store highly sensitive web traffic. Because it handles the decryption of traffic natively on the user's machine, securing the tool's infrastructure is paramount to prevent the user from being compromised by their own proxy.

## 2. Threat Landscape & Attack Surfaces

### 2.1 Malicious Upstream Servers
- **Threat:** A malicious web server sends a deliberately malformed, infinitely large, or painfully slow HTTP response to crash the proxy or exploit a buffer overflow.
- **Mitigation:** Rely entirely on the heavily tested `llhttp` Node.js parser for HTTP semantics. Implement strict memory limits for `resp_body` caching to prevent Out-Of-Memory (OOM) attacks (e.g., Slowloris style attacks from the server side).

### 2.2 SQL Injection (Local)
- **Threat:** Malicious hostnames or header values returned by a server could be crafted to inject SQL commands into the history database.
- **Mitigation:** Absolute strict usage of `sqlite3_prepare_v2` (prepared statements) for all Database operations. String concatenation in SQL is strictly prohibited in the codebase.

## 3. Key & Secret Management (Root CA)

### 3.1 The Root CA Private Key
BurpTUI generates a custom Root CA (`ca.key` and `ca.crt`) to sign TLS certificates on the fly. 
- **Risk:** If an attacker gains access to `ca/ca.key`, they can actively Man-In-The-Middle all of the user's HTTPS traffic anywhere on the internet, because the user has installed `ca.crt` into their system/browser trust store.
- **Management Strategy:**
  - `ca.key` must be generated with restricted file permissions (e.g., `chmod 600` / read-write by owner only).
  - The `ca/` directory must be strictly listed in `.gitignore` to prevent accidental commits of a developer's private CA to a public repository.
  - The user should be advised in the documentation to delete `ca.key` and `ca.crt` (and remove it from their browser) if they uninstall the tool.

### 3.2 Dynamic Certificate Caching
- **Risk:** Leaf certificates and their private keys stored in the `CertCache` in memory could be dumped.
- **Management Strategy:** Leaf certificates are ephemeral. They exist only in RAM (`std::unordered_map` holding OpenSSL `SSL_CTX` pointers) and are never written to disk. They disappear when the proxy is closed.

## 4. Privacy & Data Storage

### 4.1 Sensitive Data Logging
- **Risk:** The `data/history.db` SQLite database stores full HTTP payloads in plaintext. This includes authentication tokens, passwords, Session Cookies, and PII.
- **Management Strategy:**
  - The `data/` directory is `.gitignore`d.
  - The tool should offer a command or UI button to "Clear History" which securely truncates or deletes the `requests` table.
  - Future enhancement: Support for SQLite encryption (e.g., SQLCipher) to encrypt the DB file at rest.

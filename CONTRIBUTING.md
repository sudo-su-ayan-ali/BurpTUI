# Contributing to BurpTUI

First off, thank you for considering contributing to BurpTUI! This document outlines the coding standards, branch conventions, and workflow required to keep the codebase clean, performant, and maintainable.

## 1. Coding Guidelines & Code Style

BurpTUI relies heavily on modern C++ (C++17/20) concepts. We follow the general [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with the following strict project-specific rules:

- **Memory Management:** Manual `new` and `delete` are strictly forbidden. Use RAII, `std::unique_ptr`, and `std::shared_ptr`.
- **Async Safety:** `Boost.Asio` handlers and background threads **must never** directly modify FTXUI components. Always push a `TrafficEvent` to the `EventQueue` which will safely notify the main thread.
- **Database Safety:** Never concatenate strings to form SQL queries. Always use `sqlite3_prepare_v2` and `sqlite3_bind_*` to prevent SQL Injection.
- **Formatting:** Code must be formatted using `clang-format`. A `.clang-format` file is provided in the repository root.

## 2. Git Branch Conventions

We use a lightweight feature-branch workflow. Do not commit directly to `main`.
Branch names should be descriptive and categorized:

- **Features:** `feature/<short-description>` (e.g., `feature/websocket-support`)
- **Bug Fixes:** `bugfix/<issue-number-or-description>` (e.g., `bugfix/issue-42-chunked-encoding`)
- **Hotfixes:** `hotfix/<description>` (For critical fixes directly branched from main)
- **Documentation:** `docs/<description>` (e.g., `docs/update-readme`)

## 3. Commit Message Rules

We adhere to the [Conventional Commits](https://www.conventionalcommits.org/) standard to auto-generate changelogs.

**Format:**
```text
<type>(<scope>): <subject>

<body>
```

**Types:**
- `feat:` A new feature.
- `fix:` A bug fix.
- `docs:` Documentation only changes.
- `style:` Changes that do not affect the meaning of the code (white-space, formatting).
- `refactor:` A code change that neither fixes a bug nor adds a feature.
- `test:` Adding missing tests or correcting existing tests.

**Example:**
```text
feat(proxy): implement ALPN HTTP/1.1 downgrade

Forces browsers to negotiate HTTP/1.1 instead of HTTP/2 over TLS,
ensuring the llhttp parser does not break on binary frames.
```

## 4. Pull Request (PR) Review Workflow

1. **Fork & Branch:** Fork the repository, clone it, and create your feature branch.
2. **Write Tests:** If you are adding a feature or fixing a bug, you must include a GoogleTest unit test covering the scenario.
3. **Run Formatting & Tests:** 
   - Ensure your code passes `clang-format`.
   - Ensure all tests pass locally (`ctest`).
4. **Open a PR:** Open a PR against the `main` branch. 
   - Provide a clear summary of the changes in the PR description.
   - Link any relevant issue numbers (e.g., `Fixes #12`).
5. **CI Pipeline:** The GitHub Actions CI pipeline will automatically build the code on Linux/macOS and run all unit/integration tests. **The pipeline must pass.**
6. **Code Review:** At least one core maintainer must review and approve the PR before it can be merged. Address any review comments by pushing new commits to your branch.

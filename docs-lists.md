# BurpTUI Documentation Checklist

Here are the essential documents typically created before starting development:

---

### 1. Product & Vision Documents (The "What" and "Why")

- **PRD (Product Requirements Document)**: Outlines the product's vision, goals, target audience, core features, user personas, and success metrics (KPIs).
- **FSD / Functional Specification Document**: Translates PRD feature goals into detailed functional behavior (e.g., precise user workflows, input validations, error state logic).
- **User Stories & Acceptance Criteria**: Breaks down features into bite-sized tasks from the user's perspective (e.g., "As a security researcher, I want to edit raw requests so that...").

---

### 2. Engineering & Design Specifications (The "How")

- **TDD / HLD (Technical Design Document / High-Level Design)**: Details system architecture, component boundaries, data flow diagrams, thread/concurrency models, and performance targets.
- **Database & Data Schema Spec**: Defines database tables, ER diagrams, data types, indexes, and storage rules.
- **API Specification**: Standardizes request/response formats, endpoints, authentication headers, error codes (e.g., OpenAPI / Swagger specs).
- **UI/UX Wireframes & Component Specs**: Design mockups, interactive prototypes (Figma), and UI state transitions.

---

### 3. Execution & Process Docs (The "Plan")

- **Project Roadmap & Milestones**: A phased timeline mapping features to milestones/weeks (e.g., MVP, Phase 2, Release candidate). See [ROADMAP_TREE.md](ROADMAP_TREE.md) and the [phases/](phases/) directory.
- **Test Plan & Quality Assurance Strategy**: Defines test coverage goals, unit/integration/E2E test setup, edge cases, and CI/CD testing pipelines.
- **Security & Threat Model**: Identifies potential attack surfaces, key/secret management, cert management, and privacy requirements.

---

### 4. Project Setup & Onboarding (The "Execution Standard")

- **README.md**: Project intro, architecture summary, prerequisite software, build commands, and quick-start instructions.
- **CONTRIBUTING.md & Coding Guidelines**: Code style, git branch conventions, commit rules, and PR review workflows.

---

### Recommended Minimal Set for Personal / Small Team Projects

If you don't want to create all 10+ documents, the essential 4-document set is:

1. PRD (Features & User Requirements)
2. Architecture Spec / TDD (Tech Stack, Data Flow, Components)
3. Database & API Spec (Data structures & Interfaces)
4. Roadmap / Task Breakdown (Phases & Milestones)

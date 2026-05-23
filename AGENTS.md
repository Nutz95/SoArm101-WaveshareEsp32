# Instructions

These instructions are for AI assistants working in this project.

Always open `@AGENTS.md` when the request:
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

## Language Policy

All code (code, documentation, etc.) MUST be written in **English** only. This ensures consistency, maintainability, and ease of collaboration for all contributors.

Chat and interactive discussions with the AI assistant may continue in French if desired, but all committed files and code must remain in English.

## Coding Rules

Coding rules (formatting, SOLID design guidance language policy and related guidelines, KISS/DRY) have been consolidated into a dedicated document for the firmware subtree.

Refer to: `CODING_RULES.md` for the authoritative, project-level coding rules. All contributors and assistants must follow the rules documented there when modifying code or OpenSpec files.

If you make changes to coding rules, update `CODING_RULES.md`.

## Architecture Context

Before changing the firmware, command flow, telemetry, or dashboard UI, read the root README and the architecture documentation:

- [README.md](README.md)
- [ESP32/docs/architecture/README.md](ESP32/docs/architecture/README.md)

Use the architecture README and diagram to understand how messages move between the dashboard, leader board, and follower board.